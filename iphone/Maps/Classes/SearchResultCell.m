
#import "SearchResultCell.h"
#import "UIColor+MapsMeColor.h"
#import "UIFont+MapsMeFonts.h"
#import "UIKitCategories.h"

static CGFloat const kOffset = 16.;

@interface SearchResultCell ()

@property (nonatomic) UILabel * typeLabel;
@property (nonatomic) UILabel * subtitleLabel;
@property (nonatomic) UILabel * distanceLabel;

@end

@implementation SearchResultCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self)
  {
    [self.contentView addSubview:self.titleLabel];
    [self.contentView addSubview:self.typeLabel];
    [self.contentView addSubview:self.subtitleLabel];
    [self.contentView addSubview:self.distanceLabel];
  }

  return self;
}

#define DISTANCE_FONT [UIFont regular14]
#define TYPE_FONT [UIFont light12]
#define SUBTITLE_FONT [UIFont light12]
#define SPACE 4

- (void)configTitleLabel
{
  [super configTitleLabel];
  self.titleLabel.numberOfLines = 0;
  self.titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  self.titleLabel.textColor = [UIColor blackPrimaryText];
}

- (NSDictionary *)selectedTitleAttributes
{
  static NSDictionary * selectedAttributes;
  if (!selectedAttributes)
    selectedAttributes = @{NSForegroundColorAttributeName : [UIColor blackPrimaryText], NSFontAttributeName : [UIFont bold16]};
  return selectedAttributes;
}

- (NSDictionary *)unselectedTitleAttributes
{
  static NSDictionary * unselectedAttributes;
  if (!unselectedAttributes)
    unselectedAttributes = @{NSForegroundColorAttributeName : [UIColor blackPrimaryText], NSFontAttributeName : [UIFont regular16]};
  return unselectedAttributes;
}

- (void)layoutSubviews
{
  [super layoutSubviews];

  self.typeLabel.size = [[self class] typeSizeWithType:self.typeLabel.text];
  self.typeLabel.maxX = self.width - kOffset;
  self.typeLabel.minY = 9;

  self.distanceLabel.width = 70;
  [self.distanceLabel sizeToIntegralFit];
  self.distanceLabel.maxX = self.width - kOffset;
  self.distanceLabel.maxY = self.height - 8;

  self.titleLabel.size = [[self class] titleSizeWithTitle:self.titleLabel.text viewWidth:self.width typeSize:self.typeLabel.size];
  self.titleLabel.minX = kOffset;
  self.titleLabel.minY = self.typeLabel.minY - 4;

  self.subtitleLabel.size = CGSizeMake(self.width - self.distanceLabel.width - 2 * kOffset - SPACE, 16);
  self.subtitleLabel.minX = kOffset;
  self.subtitleLabel.maxY = self.distanceLabel.maxY;

  self.separatorView.width = self.width - kOffset;
  self.separatorView.minX = kOffset;
}

+ (CGSize)typeSizeWithType:(NSString *)type
{
  return [type sizeWithDrawSize:CGSizeMake(150, 22) font:TYPE_FONT];
}

+ (CGSize)titleSizeWithTitle:(NSString *)title viewWidth:(CGFloat)width typeSize:(CGSize)typeSize
{
  CGSize const titleDrawSize = CGSizeMake(width - typeSize.width - 2 * kOffset - SPACE, 300);
  return [title sizeWithDrawSize:titleDrawSize font:[UIFont bold16]];
}

+ (CGFloat)cellHeightWithTitle:(NSString *)title type:(NSString *)type subtitle:(NSString *)subtitle distance:(NSString *)distance viewWidth:(CGFloat)width
{
  CGSize const typeSize = [self typeSizeWithType:type];
  return MAX(50, [self titleSizeWithTitle:title viewWidth:width typeSize:typeSize].height + ([subtitle length] ? 29 : 15));
}

- (UILabel *)typeLabel
{
  if (!_typeLabel)
  {
    _typeLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _typeLabel.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleBottomMargin;
    _typeLabel.font = TYPE_FONT;
    _typeLabel.backgroundColor = [UIColor clearColor];
    _typeLabel.textColor = [UIColor blackSecondaryText];
    _typeLabel.textAlignment = NSTextAlignmentRight;
  }
  return _typeLabel;
}

- (UILabel *)distanceLabel
{
  if (!_distanceLabel)
  {
    _distanceLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _distanceLabel.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleBottomMargin;
    _distanceLabel.textColor = [UIColor blackPrimaryText];
    _distanceLabel.backgroundColor = [UIColor clearColor];
    _distanceLabel.font = DISTANCE_FONT;
    _distanceLabel.textAlignment = NSTextAlignmentRight;
  }
  return _distanceLabel;
}

- (UILabel *)subtitleLabel
{
  if (!_subtitleLabel)
  {
    _subtitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _subtitleLabel.backgroundColor = [UIColor clearColor];
    _subtitleLabel.textColor = [UIColor blackSecondaryText];
    _subtitleLabel.font = SUBTITLE_FONT;
  }
  return _subtitleLabel;
}

@end
