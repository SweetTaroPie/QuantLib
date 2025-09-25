#include "fdmvanilladoublebarriersstepcondition.h"
#include <ql/methods/finitedifferences/operators/fdmlinearoplayout.hpp>

namespace QuantLib {
    FdmVanillaDoubleBarrierCondition::FdmVanillaDoubleBarrierCondition(
        const Time& t,
        const ext::shared_ptr<FdmMesher>& mesher,
        const double& lowerBound,
        const double& upperBound,
        const double& lowerRebate,
        const double& upperRebate,
        bool isActive,
        KnockType type,
        bool atMaturity,
        bool isBucketedDeltaAtMaturity,
        const ext::shared_ptr<StrikedTypePayoff>& payoff)
    : FdmSnapshotCondition(t), type_(type), mesher_(mesher), lowerBound_(lowerBound),
      upperBound_(upperBound), lowerRebate_(lowerRebate), upperRebate_(upperRebate),
      payoff_(payoff), isActive_(isActive), atMaturity_(atMaturity),
      isBucketedDeltaAtMaturity_(isBucketedDeltaAtMaturity) {
        QL_REQUIRE(
            upperBound_ > lowerBound_,
            "FdmVanillaDoubleBarrierCondition: upper bound must be greater than lower bound");
    }

    const Array& FdmVanillaDoubleBarrierCondition::getValues() const {
        return values_;
    }

    void FdmVanillaDoubleBarrierCondition::applyTo(Array& a, Time t) const {
        if (getTime() == t) {

            ext::shared_ptr<FdmLinearOpLayout> layout = mesher_->layout();

            Real strike = payoff_->strike();
            Option::Type optionType = payoff_->optionType();

            // Real bumpSize = 0.0001 * t / maturity_;
            Real bumpSize = 0.0001;

            if (type_ == KnockType::Out) {

                if (atMaturity_) {

                    for (FdmLinearOpIterator iter = layout->begin(); iter != layout->end();
                         ++iter) {
                        Real innerValue = exp(mesher_->location(iter, 0));

                        if (isActive_) {
                            innerValue = isBucketedDeltaAtMaturity_ ? innerValue * (1 + bumpSize) :
                                                                      innerValue;

                            if (innerValue <= lowerBound_ + 1e-8) {
                                a[iter.index()] = lowerRebate_;
                            } else if (innerValue >= upperBound_ - 1e-8) {
                                a[iter.index()] = upperRebate_;
                            } else {
                            }
                        }
                    }
                } else {
                    // not at maturity

                    for (FdmLinearOpIterator iter = layout->begin(); iter != layout->end();
                         ++iter) {
                        Real innerValue = exp(mesher_->location(iter, 0));

                        if (isActive_) {
                            if (innerValue <= lowerBound_ + 1e-8) {
                                a[iter.index()] = lowerRebate_;
                            } else if (innerValue >= upperBound_ - 1e-8) {
                                a[iter.index()] = upperRebate_;
                            } else {
                            }
                        }
                    }
                }
            } else {
                // In

                for (FdmLinearOpIterator iter = layout->begin(); iter != layout->end(); ++iter) {
                    Real innerValue = exp(mesher_->location(iter, 0));

                    if (atMaturity_) {
                        if (isBucketedDeltaAtMaturity_) {
                            a[iter.index()] =
                                optionType == Option::Type::Call ?
                                    std::max(innerValue * (1 + bumpSize) - strike, 0.0) :
                                    std::max(strike - innerValue * (1 + bumpSize), 0.0);
                        }
                    }
                }

                values_ = a;
            }
        }
    }
}