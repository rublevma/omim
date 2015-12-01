#include "indexer/search_index_builder.hpp"

#include "search/reverse_geocoder.hpp"

#include "indexer/categories_holder.hpp"
#include "indexer/classificator.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/features_vector.hpp"
#include "indexer/index.hpp"
#include "indexer/search_delimiters.hpp"
#include "indexer/search_index_values.hpp"
#include "indexer/search_string_utils.hpp"
#include "indexer/search_trie.hpp"
#include "indexer/trie_builder.hpp"
#include "indexer/types_skipper.hpp"

#include "search/search_common.hpp"

#include "defines.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"
#include "coding/reader_writer_ops.hpp"
#include "coding/writer.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_add.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include "std/algorithm.hpp"
#include "std/fstream.hpp"
#include "std/initializer_list.hpp"
#include "std/limits.hpp"
#include "std/unordered_map.hpp"
#include "std/vector.hpp"

#define SYNONYMS_FILE "synonyms.txt"


namespace
{
class SynonymsHolder
{
  unordered_multimap<string, string> m_map;

public:
  SynonymsHolder(string const & fPath)
  {
    ifstream stream(fPath.c_str());

    string line;
    vector<string> tokens;

    while (stream.good())
    {
      std::getline(stream, line);
      if (line.empty())
        continue;

      tokens.clear();
      strings::Tokenize(line, ":,", MakeBackInsertFunctor(tokens));

      if (tokens.size() > 1)
      {
        strings::Trim(tokens[0]);
        for (size_t i = 1; i < tokens.size(); ++i)
        {
          strings::Trim(tokens[i]);
          // synonym should not has any spaces
          ASSERT ( tokens[i].find_first_of(" \t") == string::npos, () );
          m_map.insert(make_pair(tokens[0], tokens[i]));
        }
      }
    }
  }

  template <class ToDo>
  void ForEach(string const & key, ToDo toDo) const
  {
    using TIter = unordered_multimap<string, string>::const_iterator;

    pair<TIter, TIter> range = m_map.equal_range(key);
    while (range.first != range.second)
    {
      toDo(range.first->second);
      ++range.first;
    }
  }
};

template <typename TKey, typename TValue>
struct FeatureNameInserter
{
  SynonymsHolder * m_synonyms;
  vector<pair<TKey, TValue>> & m_keyValuePairs;
  TValue m_val;

  FeatureNameInserter(SynonymsHolder * synonyms, vector<pair<TKey, TValue>> & keyValuePairs)
    : m_synonyms(synonyms), m_keyValuePairs(keyValuePairs)
  {
  }

  void AddToken(signed char lang, strings::UniString const & s) const
  {
    strings::UniString key;
    key.reserve(s.size() + 1);
    key.push_back(static_cast<uint8_t>(lang));
    key.append(s.begin(), s.end());

    m_keyValuePairs.emplace_back(key, m_val);
  }

private:
  using TTokensArray = buffer_vector<strings::UniString, 32>;

  class PushSynonyms
  {
    TTokensArray & m_tokens;

  public:
    PushSynonyms(TTokensArray & tokens) : m_tokens(tokens) {}
    void operator() (string const & utf8str) const
    {
      m_tokens.push_back(search::NormalizeAndSimplifyString(utf8str));
    }
  };

public:
  bool operator()(signed char lang, string const & name) const
  {
    strings::UniString const uniName = search::NormalizeAndSimplifyString(name);

    // split input string on tokens
    buffer_vector<strings::UniString, 32> tokens;
    SplitUniString(uniName, MakeBackInsertFunctor(tokens), search::Delimiters());

    // add synonyms for input native string
    if (m_synonyms)
      m_synonyms->ForEach(name, PushSynonyms(tokens));

    int const maxTokensCount = search::MAX_TOKENS - 1;
    if (tokens.size() > maxTokensCount)
    {
      LOG(LWARNING, ("Name has too many tokens:", name));
      tokens.resize(maxTokensCount);
    }

    for (size_t i = 0; i < tokens.size(); ++i)
      AddToken(lang, tokens[i]);

    return true;
  }
};

template <typename TValue>
struct ValueBuilder;

template <>
struct ValueBuilder<FeatureWithRankAndCenter>
{
  ValueBuilder() = default;

  void MakeValue(FeatureType const & ft, feature::TypesHolder const & types, uint32_t index,
                 FeatureWithRankAndCenter & v) const
  {
    v.m_featureId = index;

    // get BEST geometry rect of feature
    v.m_pt = feature::GetCenter(ft);
    v.m_rank = feature::GetSearchRank(types, v.m_pt, ft.GetPopulation());
  }
};

template <>
struct ValueBuilder<FeatureIndexValue>
{
  ValueBuilder() = default;

  void MakeValue(FeatureType const & /* f */, feature::TypesHolder const & /* types */,
                 uint32_t index, FeatureIndexValue & value) const
  {
    value.m_featureId = index;
  }
};

template <typename TKey, typename TValue>
class FeatureInserter
{
  SynonymsHolder * m_synonyms;
  vector<pair<TKey, TValue>> & m_keyValuePairs;

  CategoriesHolder const & m_categories;

  pair<int, int> m_scales;

  ValueBuilder<TValue> const & m_valueBuilder;

public:
  FeatureInserter(SynonymsHolder * synonyms, vector<pair<TKey, TValue>> & keyValuePairs,
                  CategoriesHolder const & catHolder, pair<int, int> const & scales,
                  ValueBuilder<TValue> const & valueBuilder)
    : m_synonyms(synonyms)
    , m_keyValuePairs(keyValuePairs)
    , m_categories(catHolder)
    , m_scales(scales)
    , m_valueBuilder(valueBuilder)
  {
  }

  void operator() (FeatureType const & f, uint32_t index) const
  {
    feature::TypesHolder types(f);

    static search::TypesSkipper skipIndex;

    skipIndex.SkipTypes(types);
    if (types.Empty())
      return;

    // Init inserter with serialized value.
    // Insert synonyms only for countries and states (maybe will add cities in future).
    FeatureNameInserter<TKey, TValue> inserter(
        skipIndex.IsCountryOrState(types) ? m_synonyms : nullptr, m_keyValuePairs);
    m_valueBuilder.MakeValue(f, types, index, inserter.m_val);

    // Skip types for features without names.
    if (!f.ForEachNameRef(inserter))
      skipIndex.SkipEmptyNameTypes(types);
    if (types.Empty())
      return;

    Classificator const & c = classif();

    // add names of categories of the feature
    for (uint32_t t : types)
    {
      // Leave only 2 level of type - for example, do not distinguish:
      // highway-primary-bridge or amenity-parking-fee.
      ftype::TruncValue(t, 2);

      // Push to index only categorized types.
      if (m_categories.IsTypeExist(t))
      {
        // Do index only for visible types in mwm.
        pair<int, int> const r = feature::GetDrawableScaleRange(t);
        CHECK(r.first <= r.second && r.first != -1, (c.GetReadableObjectName(t)));

        if (r.second >= m_scales.first && r.first <= m_scales.second)
        {
          inserter.AddToken(search::kCategoriesLang,
                            search::FeatureTypeToString(c.GetIndexForType(t)));
        }
      }
    }
  }
};

template <typename TKey, typename TValue>
void AddFeatureNameIndexPairs(FeaturesVectorTest & features, CategoriesHolder & categoriesHolder,
                              vector<pair<TKey, TValue>> & keyValuePairs)
{
  feature::DataHeader const & header = features.GetHeader();

  ValueBuilder<TValue> valueBuilder;

  unique_ptr<SynonymsHolder> synonyms;
  if (header.GetType() == feature::DataHeader::world)
    synonyms.reset(new SynonymsHolder(GetPlatform().WritablePathForFile(SYNONYMS_FILE)));

  features.GetVector().ForEach(FeatureInserter<TKey, TValue>(
      synonyms.get(), keyValuePairs, categoriesHolder, header.GetScaleRange(), valueBuilder));

  ReaderSource<ModelReaderPtr> src = features.GetReader(SEARCH_TOKENS_FILE_TAG);
  uint64_t index = 0;
  uint64_t address = 0, missing = 0;
  map<size_t, size_t> bounds;

  Index mwmIndex;
  /// @ todo Make some better solution, or legalize MakeTemporary.
  mwmIndex.RegisterMap(platform::LocalCountryFile::MakeTemporary(features.GetFilePath()));
  search::ReverseGeocoder rgc(mwmIndex);

  while (src.Size() > 0)
  {
    feature::AddressData data;
    data.Deserialize(src);

    string street;
    search::GetStreetNameAsKey(data.Get(feature::AddressData::STREET), street);
    if (!street.empty())
    {
      FeatureType ft;
      features.GetVector().GetByIndex(index, ft);

      using TStreet = search::ReverseGeocoder::Street;
      vector<TStreet> streets;
      rgc.GetNearbyStreets(ft, street, streets);

      size_t const ind = rgc.GetMatchedStreetIndex(streets);
      if (ind == streets.size())
      {
        ++missing;
        //LOG(LWARNING, ("No street found for address", street, ft));
      }
      else
      {
        ++bounds[ind];
      }

      ++address;
    }

    ++index;
  }

  LOG(LINFO, ("Address: Matched percent", 100 * (1.0 - missing/double(address))));
  LOG(LINFO, ("Address: Upper bounds", bounds));

  /// @todo Delete SEARCH_TOKENS_FILE_TAG section in production code.
}
}  // namespace

namespace indexer
{
bool BuildSearchIndexFromDataFile(string const & filename, bool forceRebuild)
{
  Platform & platform = GetPlatform();

  FilesContainerR readContainer(platform.GetReader(filename));
  if (readContainer.IsExist(SEARCH_INDEX_FILE_TAG) && !forceRebuild)
    return true;

  string mwmName = filename;
  my::GetNameFromFullPath(mwmName);
  my::GetNameWithoutExt(mwmName);
  string const indexFilePath = platform.WritablePathForFile(mwmName + "." + string(SEARCH_INDEX_FILE_TAG) + ".tmp");
  MY_SCOPE_GUARD(indexFileGuard, bind(&FileWriter::DeleteFileX, indexFilePath));

  try
  {
    {
      FileWriter indexWriter(indexFilePath);
      BuildSearchIndex(readContainer, indexWriter);
      LOG(LINFO, ("Search index size =", indexWriter.Size()));
    }
    {
      FilesContainerW writeContainer(readContainer.GetFileName(), FileWriter::OP_WRITE_EXISTING);
      FileWriter writer = writeContainer.GetWriter(SEARCH_INDEX_FILE_TAG);
      rw_ops::Reverse(FileReader(indexFilePath), writer);
    }
  }
  catch (Reader::Exception const & e)
  {
    LOG(LERROR, ("Error while reading file:", e.Msg()));
    return false;
  }
  catch (Writer::Exception const & e)
  {
    LOG(LERROR, ("Error writing index file:", e.Msg()));
    return false;
  }

  return true;
}

void BuildSearchIndex(FilesContainerR & container, Writer & indexWriter)
{
  using TKey = strings::UniString;
  using TValue = FeatureIndexValue;

  Platform & platform = GetPlatform();

  LOG(LINFO, ("Start building search index for", container.GetFileName()));
  my::Timer timer;

  CategoriesHolder categoriesHolder(platform.GetReader(SEARCH_CATEGORIES_FILE_NAME));

  FeaturesVectorTest features(container);
  auto codingParams = trie::GetCodingParams(features.GetHeader().GetDefCodingParams());
  SingleValueSerializer<TValue> serializer(codingParams);

  vector<pair<TKey, TValue>> searchIndexKeyValuePairs;
  AddFeatureNameIndexPairs(features, categoriesHolder, searchIndexKeyValuePairs);

  sort(searchIndexKeyValuePairs.begin(), searchIndexKeyValuePairs.end());
  LOG(LINFO, ("End sorting strings:", timer.ElapsedSeconds()));

  trie::Build<Writer, TKey, ValueList<TValue>, SingleValueSerializer<TValue>>(
      indexWriter, serializer, searchIndexKeyValuePairs);

  LOG(LINFO, ("End building search index, elapsed seconds:", timer.ElapsedSeconds()));
}
}  // namespace indexer