#ifndef FD_MUREX_BARRIER_ENGINE_HPP
#define FD_MUREX_BARRIER_ENGINE_HPP

#include <ql/instruments/barriers/barrieroption.hpp>
#include <ql/methods/finitedifferences/all.hpp>

namespace QuantLib {

    //! Finite-Differences Single/Double Barrier Option engine

    /*!
        \ingroup barrierengines
    */
    class FdMurexBarrierEngine : public BarrierOption::engine {
      public:
        explicit FdMurexBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                                      Size tGrid = 100,
                                      Size xGrid = 100,
                                      Size dampingSteps = 0,
                                      const FdmSchemeDesc& schemeDesc = FdmSchemeDesc::Douglas(),
                                      bool isBucketedDeltaAtMaturity = false,
                                      bool localVol = false,
                                      Real illegalLocalVolOverwrite = -Null<Real>());

        void calculate() const override;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        Size tGrid_, xGrid_, dampingSteps_;
        FdmSchemeDesc schemeDesc_;
        bool isBucketedDeltaAtMaturity_;
        bool localVol_;
        Real illegalLocalVolOverwrite_;
    };
}
#endif