#include <ql/pricingengines/barrier/mcsimplebarrierengine.hpp>
#include <utility>

namespace QuantLib {

    SimpleBarrierPathPricer::SimpleBarrierPathPricer(
        const ext::shared_ptr<BarrierBase> barrierBase,
        Option::Type type,
        Real strike,
        std::vector<DiscountFactor> discounts,
        ext::shared_ptr<StochasticProcess1D> diffProcess,
        Size mBegin,
        Size mEnd)
    // barrierType_(barrierType), barrier_(barrier), rebate_(rebate),
    : Barrier_(barrierBase), diffProcess_(std::move(diffProcess)), monitoringBegin_(mBegin),
      monitoringEnd_(mEnd), payoff_(type, strike), discounts_(std::move(discounts)) {

        QL_REQUIRE(strike >= 0.0, "strike less than zero not allowed");
        /*
        QL_REQUIRE(Barrier_->barrier()>0.0,
                  "barrier less/equal zero not allowed");*/

        QL_REQUIRE(mBegin <= mEnd, "barrier start time greater than barrier end time");
    }

    Real SimpleBarrierPathPricer::operator()(const Path& path) const {
        static Size null = Null<Size>();
        Size n = path.length();
        QL_REQUIRE(n > 1, "the path cannot be empty");

        bool isOptionActive = false;
        Size knockMode = null;
        Real asset_price = path.back();
        Size i;

        Real runningMin =
            *std::min_element(path.begin() + monitoringBegin_, path.begin() + monitoringEnd_);
        Real runningMax =
            *std::max_element(path.begin() + monitoringBegin_, path.begin() + monitoringEnd_);

        isOptionActive = Barrier_->activated(runningMax, runningMin);

        if (isOptionActive) {
            return payoff_(asset_price) * discounts_.back();
        } else {
            return Barrier_->rebate() * discounts_.back();
        }
    }

}