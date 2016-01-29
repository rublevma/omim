#import "MWMButton.h"
#import "UIColor+MapsMeColor.h"

namespace
{
  NSString * const kDefaultPattern = @"%@_%@";
  NSString * const kHighlightedPattern = @"%@_highlighted_%@";
  NSString * const kSelectedPattern = @"%@_selected_%@";
} // namespace

@implementation MWMButton

- (void)setImageName:(NSString *)imageName
{
  _imageName = imageName;
  [self setDefaultImages];
}

- (void)refresh
{
  [self changeColoringToOpposite];
  [super refresh];
}

- (void)setColoring:(MWMButtonColoring)coloring
{
  _coloring = coloring;
  [self.imageView makeImageAlwaysTemplate];
  [self setDefaultTintColor];
}

- (void)changeColoringToOpposite
{
  if (self.coloring == MWMButtonColoringOther)
  {
    if (self.imageName)
    {
      [self setDefaultImages];
      self.imageView.image = [self imageForState:self.state];
    }
    return;
  }
  if (self.state == UIControlStateNormal)
    [self setDefaultTintColor];
  else if (self.state == UIControlStateHighlighted)
    [self setHighlighted:YES];
  else if (self.state == UIControlStateSelected)
    [self setSelected:YES];
}

- (void)setDefaultImages
{
  NSString * postfix = [UIColor isNightMode] ? @"dark" : @"light";
  [self setImage:[UIImage imageNamed:[NSString stringWithFormat:kDefaultPattern, self.imageName, postfix]] forState:UIControlStateNormal];
  [self setImage:[UIImage imageNamed:[NSString stringWithFormat:kHighlightedPattern, self.imageName, postfix]] forState:UIControlStateHighlighted];
  [self setImage:[UIImage imageNamed:[NSString stringWithFormat:kSelectedPattern, self.imageName, postfix]] forState:UIControlStateSelected];
}

- (void)setHighlighted:(BOOL)highlighted
{
  [super setHighlighted:highlighted];
  [self.imageView makeImageAlwaysTemplate];
  if (highlighted)
  {
    switch (self.coloring)
    {
      case MWMButtonColoringBlue:
        self.tintColor = [UIColor linkBlueDark];
        break;
      case MWMButtonColoringBlack:
        self.tintColor = [UIColor blackHintText];
        break;
      case MWMButtonColoringGray:
        self.tintColor = [UIColor blackDividers];
        break;
      case MWMButtonColoringOther:
        break;
    }
  }
  else
  {
    if (self.selected)
      return;
    [self setDefaultTintColor];
  }
}

- (void)setSelected:(BOOL)selected
{
  [super setSelected:selected];
  [self.imageView makeImageAlwaysTemplate];
  if (selected)
  {
    switch (self.coloring)
    {
      case MWMButtonColoringBlack:
        self.tintColor = [UIColor linkBlue];
        break;
      case MWMButtonColoringBlue:
      case MWMButtonColoringOther:
      case MWMButtonColoringGray:
        break;
    }
  }
  else
  {
    [self setDefaultTintColor];
  }
}

- (void)setDefaultTintColor
{
  switch (self.coloring)
  {
    case MWMButtonColoringBlack:
      self.tintColor = [UIColor blackSecondaryText];
      break;
    case MWMButtonColoringBlue:
      self.tintColor = [UIColor linkBlue];
      break;
    case MWMButtonColoringGray:
      self.tintColor = [UIColor blackHintText];
      break;
    case MWMButtonColoringOther:
      self.imageView.image = [self imageForState:UIControlStateNormal];
      break;
  }
}

- (void)setColoringName:(NSString *)coloring
{
  if ([coloring isEqualToString:@"MWMBlue"])
    self.coloring = MWMButtonColoringBlue;
  else if ([coloring isEqualToString:@"MWMBlack"])
    self.coloring = MWMButtonColoringBlack;
  else if ([coloring isEqualToString:@"MWMOther"])
    self.coloring = MWMButtonColoringOther;
  else if ([coloring isEqualToString:@"MWMGray"])
    self.coloring = MWMButtonColoringGray;
  else
    NSAssert(false, @"Invalid UIButton's coloring!");
}

@end