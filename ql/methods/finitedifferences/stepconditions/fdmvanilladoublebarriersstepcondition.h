#ifndef QALIB_FDM_VANILLA_BARRIER_STEP_CONDITIONS
#define QALIB_FDM_VANILLA_BARRIER_STEP_CONDITIONS

#include <ql/methods/finitedifferences/stepconditions/all.hpp>
#include <ql/time/daycounter.hpp>

namespace QuantLib {
    class FdmVanillaDoubleBarrierCondition : public FdmSnapshotCondition {
      public:
        enum KnockType { In, Out };

      public:
        FdmVanillaDoubleBarrierCondition(
            const Time& t,
            const ext::shared_ptr<FdmMesher>& mesher,
            const double& lowerBound,
            const double& upperBound,
            const double& lowerRebate,
            const double& upperRebate,
            bool isActive = false,
            KnockType type = KnockType::Out,
            bool atMaturity = false,
            bool isBucketedDeltaAtMaturity = false,
            const ext::shared_ptr<StrikedTypePayoff>& payoff = nullptr);

      public:
        void applyTo(Array& a, Time t) const override;

        const Array& getValues() const;

      private:
        KnockType type_;

        ext::shared_ptr<FdmMesher> mesher_;

        ext::shared_ptr<StrikedTypePayoff> payoff_;

        double lowerBound_, upperBound_;

        double lowerRebate_, upperRebate_;

        Array vanillaValues_;

        bool isBucketedDeltaAtMaturity_;

        bool atMaturity_;

        bool isActive_;

        // mutable Array barrierStatus_;
        mutable Array values_;
    };
}

#endif