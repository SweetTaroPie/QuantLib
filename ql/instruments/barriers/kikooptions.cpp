#include "kikooptions.h"

namespace QuantLib {

    KikoOption::KikoOption(const ext::shared_ptr<KIKOBarrier>& barrier,
                           const ext::shared_ptr<StrikedTypePayoff>& payoff,
                           const ext::shared_ptr<Exercise>& exercise)
    : OneAssetOption(payoff, exercise), kikoBarrier_(barrier) {}

    KikoOption::KikoOption(const ext::shared_ptr<KIKOBarrier>& barrier,
                           const ext::shared_ptr<StrikedTypePayoff>& lowerpayoff,
                           const ext::shared_ptr<StrikedTypePayoff>& upperpayoff,
                           const ext::shared_ptr<Exercise>& exercise)
    : OneAssetOption(lowerpayoff, exercise), kikoBarrier_(barrier), lowerpayoff_(lowerpayoff),
      upperpayoff_(upperpayoff) {}

    KikoOption::arguments::arguments() {}

    void KikoOption::setupArguments(PricingEngine::arguments* args) const {
        OneAssetOption::setupArguments(args);

        auto* moreArgs = dynamic_cast<KikoOption::arguments*>(args);

        moreArgs->kikoBarrier_ = kikoBarrier_;

        moreArgs->lowerpayoff_ = lowerpayoff_;
        moreArgs->upperpayoff_ = upperpayoff_;
    }

    void KikoOption::arguments::validate() const {}
}