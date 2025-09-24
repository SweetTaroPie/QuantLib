#ifndef FDM_MUREX_KIKO_OPTION_ENGINE_H
#define FDM_MUREX_KIKO_OPTION_ENGINE_H

#include <ql/instruments/barriers/kikooptions.h>
#include <ql/methods/finitedifferences/all.hpp>

namespace QuantLib {

    //! Finite-Differences Single/Double Barrier Option engine

    /*!
        \ingroup barrierengines
    */
    class FdMurexKikoSingleInEngine : public KikoOption::engine {
      public:
        explicit FdMurexKikoSingleInEngine(
            ext::shared_ptr<GeneralizedBlackScholesProcess> process,
            Size tGrid = 100,
            Size xGrid = 100,
            Size dampingSteps = 0,
            const FdmSchemeDesc& schemeDesc = FdmSchemeDesc::Douglas(),
            bool isPaymentAtMaturity = true,
            bool isBucketedDeltaAtMaturity = false,
            // bool isPaymentInAsset = true,
            bool localVol = false,
            Real illegalLocalVolOverwrite = -Null<Real>());

        /*
        ext::shared_ptr<FdmStepConditionComposite> KnockOutStepConditionSet(
            const ext::shared_ptr<BarrierBase>& barrier,
            const Date& referenceDate) const;*/

        std::vector<Time> BarrierTimes(const ext::shared_ptr<BarrierBase>& barrier,
                                       const Date& referenceDate) const;

        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        Size tGrid_, xGrid_, dampingSteps_;
        FdmSchemeDesc schemeDesc_;
        bool isPaymentAtMaturity_;
        bool isBucketedDeltaAtMaturity_;
        // bool isPaymentInAsset_;
        bool localVol_;
        Real illegalLocalVolOverwrite_;
    };

    class FdMurexKikoDoubleInEngine : public KikoOption::engine {
      public:
        explicit FdMurexKikoDoubleInEngine(
            ext::shared_ptr<GeneralizedBlackScholesProcess> process,
            Size tGrid = 100,
            Size xGrid = 100,
            Size dampingSteps = 0,
            const FdmSchemeDesc& schemeDesc = FdmSchemeDesc::Douglas(),
            bool isPaymentAtMaturity = false,
            bool isBucketedDeltaAtMaturity = false,
            bool localVol = false,
            Real illegalLocalVolOverwrite = -Null<Real>());

        /*
        ext::shared_ptr<FdmStepConditionComposite> KnockOutStepConditionSet(
            const ext::shared_ptr<BarrierBase>& barrier,
            const Date& referenceDate) const;*/

        std::vector<Time> BarrierTimes(const ext::shared_ptr<BarrierBase>& barrier,
                                       const Date& referenceDate) const;

        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        Size tGrid_, xGrid_, dampingSteps_;
        FdmSchemeDesc schemeDesc_;
        bool isPaymentAtMaturity_;
        bool isBucketedDeltaAtMaturity_;
        bool localVol_;
        Real illegalLocalVolOverwrite_;
    };
}

#endif