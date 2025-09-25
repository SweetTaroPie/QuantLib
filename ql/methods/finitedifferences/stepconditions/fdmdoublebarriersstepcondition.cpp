#include "fdmdoublebarriersstepcondition.h"
#include <ql/methods/finitedifferences/operators/fdmlinearoplayout.hpp>

namespace QuantLib {
    FdmDoubleBarrierCondition::FdmDoubleBarrierCondition(
        const Time& t,
        const ext::shared_ptr<FdmMesher>& mesher,
        const double& lowerBound,
        const double& upperBound,
        const double& lowerRebate,
        const double& upperRebate,
        const bool& isActive,
        KnockType type,
        const Array& vanillaValues,
        bool atMaturity,
        bool isBucketedDeltaAtMaturity,
        const ext::shared_ptr<StrikedTypePayoff>& payoff)
    : FdmSnapshotCondition(t), type_(type), mesher_(mesher), lowerBound_(lowerBound),
      upperBound_(upperBound), lowerRebate_(lowerRebate), upperRebate_(upperRebate),
      isActive_(isActive), vanillaValues_(vanillaValues), atMaturity_(atMaturity),
      isBucketedDeltaAtMaturity_(isBucketedDeltaAtMaturity), payoff_(payoff) {
        QL_REQUIRE(upperBound_ > lowerBound_,
                   "FdmDoubleBarrierCondition: upper bound must be greater than lower bound");
    }

    const Array& FdmDoubleBarrierCondition::getValues() const {
        return values_;
    }

    void FdmDoubleBarrierCondition::applyTo(Array& a, Time t) const {
        if (getTime() == t) {

            ext::shared_ptr<FdmLinearOpLayout> layout = mesher_->layout();

            QL_REQUIRE(vanillaValues_.size() == layout->size(),
                       std::string("FdmDoubleBarrierCondition: vanillaValues_ size unmatch") +
                           ", vanillaValues_.size = " + std::to_string(vanillaValues_.size()) +
                           ", layout.size = " + std::to_string(layout->size()) + 
                           ", t = " + std::to_string(t)
            );

                Real strike = payoff_->strike();
                Option::Type optionType = payoff_->optionType();

                // Real bumpSize = 0.0001 * t / maturity_;
                Real bumpSize = 0.0001;

                if (type_ == KnockType::Out) {

                    for (FdmLinearOpIterator iter = layout->begin(); iter != layout->end();
                         ++iter) {
                        Real innerValue = exp(mesher_->location(iter, 0));

                        a[iter.index()] = vanillaValues_[iter.index()];
                    }
                } else {
                    // In

                    if (atMaturity_) {

                        if (isActive_) {

                            for (FdmLinearOpIterator iter = layout->begin(); iter != layout->end();
                                 ++iter) {
                                Real innerValue = exp(mesher_->location(iter, 0));

                                innerValue = isBucketedDeltaAtMaturity_ ?
                                                 innerValue * (1 + bumpSize) :
                                                 innerValue;

                                if // if up-in
                                    (lowerBound_ == QL_MIN_REAL) {

                                    if (innerValue >= upperBound_ - 1e-8) {
                                        a[iter.index()] = vanillaValues_[iter.index()];
                                    } else {
                                        a[iter.index()] = 0.0;
                                    }
                                }
                                // if down-in
                                else if (upperBound_ == QL_MAX_REAL) {
                                    if (innerValue <= lowerBound_ + 1e-8) {
                                        a[iter.index()] = vanillaValues_[iter.index()];
                                    } else {
                                        a[iter.index()] = 0.0;
                                    }
                                } else {
                                    // In
                                    if (innerValue >= upperBound_ - 1e-8 ||
                                        innerValue <= lowerBound_ + 1e-8) {
                                        a[iter.index()] = vanillaValues_[iter.index()];
                                    } else {
                                        a[iter.index()] = 0.0;
                                    }
                                }
                            }
                        } else {

                            for (FdmLinearOpIterator iter = layout->begin(); iter != layout->end();
                                 ++iter) {
                                a[iter.index()] = 0.0;
                            }
                        }
                    } else {
                        // not in the middle of rolling back
                        if (isActive_) {

                            for (FdmLinearOpIterator iter = layout->begin(); iter != layout->end();
                                 ++iter) {
                                Real innerValue = exp(mesher_->location(iter, 0));

                                if // if up-in
                                    (lowerBound_ == QL_MIN_REAL) {

                                    if (innerValue >= upperBound_ - 1e-8) {
                                        a[iter.index()] = vanillaValues_[iter.index()];
                                    } else {
                                        // a[iter.index()] = 0.0;
                                    }
                                }
                                // if down-in
                                else if (upperBound_ == QL_MAX_REAL) {
                                    if (innerValue <= lowerBound_ + 1e-8) {
                                        a[iter.index()] = vanillaValues_[iter.index()];
                                    } else {
                                        // a[iter.index()] = 0.0;
                                    }
                                } else {
                                    // In
                                    if (innerValue >= upperBound_ - 1e-8 ||
                                        innerValue <= lowerBound_ + 1e-8) {
                                        a[iter.index()] = vanillaValues_[iter.index()];
                                    } else {
                                        // a[iter.index()] = 0.0;
                                    }
                                }
                            }
                        }
                    }
                }

                values_ = a;
        }
    }
}