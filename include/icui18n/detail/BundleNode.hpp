#pragma once

#include <memory>
#include <string>

#include <unicode/resbund.h>

namespace icui18n::detail {

// One node in the per-object bundle chain.
// The chain runs most-derived → ... → hierarchy root, mirroring the class
// hierarchy. Key lookup walks head → parent; ICU handles locale fallback
// (e.g. en_US → en → root) within each individual node automatically.
// Both bundle_root and bundle_name are retained so the chain can be rebuilt
// in-place when the locale changes.
struct BundleNode
{
    std::string          bundle_root;   // filesystem root for this class's bundle
    std::string          bundle_name;   // e.g. "com/example/ServiceBundle"
    icu::ResourceBundle  bundle;
    std::unique_ptr<BundleNode> parent;
};

} // namespace icui18n::detail
