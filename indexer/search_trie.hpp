#pragma once

#include "indexer/geometry_serialization.hpp"
#include "indexer/search_index_values.hpp"
#include "indexer/trie.hpp"
#include "indexer/trie_reader.hpp"

#include "coding/reader.hpp"

namespace search
{
static const uint8_t kCategoriesLang = 128;
static const uint8_t kPointCodingBits = 20;
}  // namespace search

namespace trie
{
inline serial::CodingParams GetCodingParams(serial::CodingParams const & orig)
{
  return serial::CodingParams(search::kPointCodingBits,
                              PointU2PointD(orig.GetBasePoint(), orig.GetCoordBits()));
}

}  // namespace trie