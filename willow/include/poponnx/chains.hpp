#ifndef GUARD_NEURALNET_CHAINS_HPP
#define GUARD_NEURALNET_CHAINS_HPP

#include <memory>
#include <vector>
#include <poponnx/names.hpp>
#include <poponnx/region.hpp>

// we currently only consider inplacing ops with 1 output. this can be
// generalised in the future if we decide it is necessary

namespace poponnx {
namespace view {

// a class for mapping a Region to another Region
// by (1) applying a filter and then (2) mapping it
class Link {
public:
  // A link with the identity region mapper, so that regmap(r) = r.
  static Link getIdentity(const Region &filter);

  Link(const Region &r_filter, const RegMap &r2r_mapper);
  Region apply(const Region &r) const { return regmap(filter.intersect(r)); }
  const Region &getFilter() const { return filter; }

private:
  Region filter;
  RegMap regmap;
};

// a sequence of Links
class Chain {
public:
  // a single indentity Link
  static Chain getIdentity(const Region &);

  Chain(const Link &l) { links = {l}; }
  Region apply(const Region &) const;
  void append(const Chain &);
  const std::vector<Link> &getLinks() { return links; }
  // Returns true when apply(a full tensor region) = empty region
  bool untraversable() const;

private:
  std::vector<Link> links;
};

// a set of parallel Chain objects
class Chains {

public:
  // a single identity Chain
  static Chains getIdentity(const Region &);
  static Chains getIdentity(const Shape &);

  // default constructor has no Chain objects
  Chains() = default;
  Chains(const Link &);
  Chains(const std::vector<Chain> &);
  Chains series(const Chains &) const;
  Chains parallel(const Chains &) const;
  Regions apply(const Region &r) const;
  bool isEmpty() const { return chain_union.size() == 0; }

private:
  // TODO : consider rather a vector of lists
  std::vector<Chain> chain_union;
};

} // namespace view
} // namespace poponnx

#endif