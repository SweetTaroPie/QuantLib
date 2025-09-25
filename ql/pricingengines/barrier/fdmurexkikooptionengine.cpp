#include "fdmurexkikooptionengine.hpp"
#include "fdmurexbarrierengine.hpp"
#include <ql/instruments/vanillaoption.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/methods/finitedifferences/stepconditions/fdmdoublebarrierstepconnection.h>
#include <ql/methods/finitedifferences/stepconditions/fdmkikostepcondition.hpp>
#include <ql/methods/finitedifferences/stepconditions/fdmvanillakikostepcondition.h>
#include <ql/methods/finitedifferences/utilities/fdmnongammaboundarycondition.hpp>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <list>
#include <utility>

namespace QuantLib {

    FdMurexKikoSingleInEngine::FdMurexKikoSingleInEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process,
        Size tGrid,
        Size xGrid,
        Size dampingSteps,
        const FdmSchemeDesc& schemeDesc,
        bool isPaymentAtMaturity,
        bool isBucketedDeltaAtMaturity,
        // bool isPaymentInAsset,
        bool localVol,
        Real illegalLocalVolOverwrite)
    : process_(std::move(process)), // explicitDividends_(false),
      tGrid_(tGrid), xGrid_(xGrid), dampingSteps_(dampingSteps), schemeDesc_(schemeDesc),
      isPaymentAtMaturity_(isPaymentAtMaturity),
      isBucketedDeltaAtMaturity_(isBucketedDeltaAtMaturity),
      // isPaymentInAsset_(isPaymentInAsset),
      localVol_(localVol), illegalLocalVolOverwrite_(illegalLocalVolOverwrite) {

        registerWith(process_);
    }

    std::vector<Time>
    FdMurexKikoSingleInEngine::BarrierTimes(const ext::shared_ptr<BarrierBase>& barrier,
                                            const Date& referenceDate) const {
        MonitoringType monitoring = barrier->monitoringType();

        Real rebate = barrier->rebate();

        std::vector<Date> discreteDates;

        std::vector<Time> barrierTimes;

        if (monitoring == MonitoringType::Discrete) {
            discreteDates = barrier->monitoringDates();
        }

        if (discreteDates.size() != 0) {
            for (auto& d : discreteDates) {
                barrierTimes.push_back(Actual365Fixed().yearFraction(referenceDate, d));
            }
        } else {
            Time start = Actual365Fixed().yearFraction(referenceDate, barrier->windowStart());
            Time end = Actual365Fixed().yearFraction(referenceDate, barrier->windowEnd());

            barrierTimes.push_back(start);

            // for continuous barrier, using daily barrier frequency
            for (Time t = start + 1.0 / 365.0; t < end; t += 1.0 / 365.0) {
                barrierTimes.push_back(t);
            }
        }

        if (barrierTimes.back() != end) {
            barrierTimes.push_back(end);
        }

        return barrierTimes;
    }

    void FdMurexKikoSingleInEngine::calculate() const {
        KIKOBarrierType kikoType = arguments_.kikobarrier_->kikoType();
        FdmVanillaKikoCondition::KIKOBarrierType vanillaKikoType =
            (kikoType == KIKOBarrierType::Any)   ? FdmVanillaKikoCondition::KIKOBarrierType::Any :
            (kikoType == KIKOBarrierType::Until) ? FdmVanillaKikoCondition::KIKOBarrierType::Until :
                                                   FdmVanillaKikoCondition::KIKOBarrierType::After;

        double spot = process_->x0();

        double L = spot / 2.0;
        double U = spot * 2.0;

        double lower_out = QL_MIN_REAL;
        double lower_in = QL_MIN_REAL;

        double upper_out = QL_MAX_REAL;
        double upper_in = QL_MAX_REAL;

        double lowerRebate = 0.0;
        double upperRebate = 0.0;

        bool isOutOption = false;

        SimpleBarrierType InType =
            ext::dynamic_pointer_cast<SimpleBarrier>(arguments_.kikobarrier_->KnockInBarrier())
                ->type();

        ext::shared_ptr<SimpleBarrier> KnockInBarrier =
            ext::dynamic_pointer_cast<SimpleBarrier>(arguments_.kikobarrier_->KnockInBarrier());

        SimpleBarrierType OutType_Simple;
        DoubleBarrierType OutType_Double;

        ext::shared_ptr<SimpleBarrier> KnockOutBarrier_Simple =
            ext::dynamic_pointer_cast<SimpleBarrier>(arguments_.kikobarrier_->KnockOutBarrier());

        ext::shared_ptr<DoubleBarrier> KnockOutBarrier_Double =
            ext::dynamic_pointer_cast<DoubleBarrier>(arguments_.kikobarrier_->KnockOutBarrier());

        bool is_OutType_Simple = false;

        if (KnockOutBarrier_Simple.get() != nullptr) {
            is_OutType_Simple = true;

            OutType_Simple = KnockOutBarrier_Simple->type();

            QL_REQUIRE(((OutType_Simple == SimpleBarrierType::DO) ||
                        (OutType_Simple == SimpleBarrierType::UO)),
                       "KO type has to be LO, UO, or Out");
        } else if (KnockOutBarrier_Double.get() != nullptr) {
            OutType_Double = KnockOutBarrier_Double->type();

            QL_REQUIRE(OutType_Double == DoubleBarrierType::Out,
                       "Lower KO type has to be LO, UO, or Out");
        } else {
            QL_FAIL("Lower KO invalid");
        }

        // [Barrier configuration logic continues...]

        Date refDate = process_->riskFreeRate()->referenceDate();

        std::vector<Time> KnockInTimes = BarrierTimes(KnockInBarrier, refDate);

        std::vector<Time> KnockOutTimes = is_OutType_Simple ?
                                              BarrierTimes(KnockOutBarrier_Simple, refDate) :
                                              BarrierTimes(KnockOutBarrier_Double, refDate);

        // 1. Mesher
        const ext::shared_ptr<StrikedTypePayoff> payoff =
            ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        const Time maturity = process_->time(arguments_.exercise->lastDate());

        lower_out = is_OutType_Simple ? (KnockOutBarrier_Simple->type() == SimpleBarrierType::DO ?
                                             KnockOutBarrier_Simple->barrier() :
                                             QL_MIN_REAL) :
                                        KnockOutBarrier_Double->lowerBarrier();

        // [Mesh setup continues...]

        std::vector<Time> monitoringTimes;
        // monitoringTimes.reserve(KnockInTimes.size() + KnockOutTimes.size() + 1);

        // monitoringTimes.push_back(0.0);
        monitoringTimes.push_back(maturity);

        monitoringTimes.insert(monitoringTimes.end(), KnockInTimes.begin(), KnockInTimes.end());
        monitoringTimes.insert(monitoringTimes.end(), KnockOutTimes.begin(), KnockOutTimes.end());

        std::sort(monitoringTimes.begin(), monitoringTimes.end());
        monitoringTimes.erase(std::unique(monitoringTimes.begin(), monitoringTimes.end()),
                              monitoringTimes.end());

        // 2. Calculator
        const ext::shared_ptr<FdmInnerValueCalculator> inCalculator(
            new FdmLogInnerValue(payoff, KnockInMesher, 0));

        // 3. Boundary conditions
        FdmBoundaryConditionSet boundaries;

        // 4. Step conditions
        QL_REQUIRE(arguments_.exercise->type() == Exercise::European,
                   "only european style option are supported");

        Date referenceDate = process_->riskFreeRate()->referenceDate();

        // insert barrier condition
        std::list<ext::shared_ptr<StepCondition<Array>>> stepConditions;
        std::list<std::vector<Time>> stoppingTimes;

        stoppingTimes.emplace_back(monitoringTimes);

        std::list<ext::shared_ptr<StepCondition<Array>>> tranlaticOptConditions;

        /* solve PDE for translated option */

        /* 1. ANY or AFTER: Knock-Out Barrier Option
           UNTIL: Vanilla Option
        */

        std::vector<ext::shared_ptr<StepCondition<Array>>> snapShots;

        // using tranlaticOptConditions to snapshot vanilla prices at each monitoring dates
        for (Size i = 0; i < monitoringTimes.size(); ++i) {

            snapShots.push_back(ext::shared_ptr<StepCondition<Array>>(new FdmVanillaKikoCondition(
                monitoringTimes[i], maturity, KnockInMesher, lower_out, upper_out, lowerRebate,
                upperRebate, payoff, vanillaKikoType,
                (i == monitoringTimes.size() - 1) ? true : false, isBucketedDeltaAtMaturity_,
                // isPaymentInAsset_,
                std::find_if(KnockInTimes.begin(), KnockInTimes.end(),
                             [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                    KnockInTimes.end(),
                std::find_if(KnockOutTimes.begin(), KnockOutTimes.end(),
                             [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                    KnockOutTimes.end(),
                isPaymentAtMaturity_)));

            tranlaticOptConditions.push_back(snapShots[i]);
        }

        ext::shared_ptr<FdmStepConditionComposite> tranConditionsSet(
            new FdmStepConditionComposite(stoppingTimes, tranlaticOptConditions));

        // [Solver configuration continues...]

        FdmSolverDesc vanillaSolverDesc = {KnockInMesher, boundaries, tranConditionsSet,
                                           inCalculator,  maturity,   tGrid_,
                                           dampingSteps_};

        const ext::shared_ptr<FdmBlackScholesSolver> vanillaSolver(
            ext::make_shared<FdmBlackScholesSolver>(
                Handle<GeneralizedBlackScholesProcess>(process_), payoff->strike(),
                vanillaSolverDesc, schemeDesc_, localVol_, illegalLocalVolOverwrite_));

        // all snap shots completed this stage
        double vanillaPV = vanillaSolver->valueAt(spot);

        // [Result calculation continues...]

        results_.value = vanillaSolver->valueAt(spot);

        Array snapValues =
            ext::dynamic_pointer_cast<FdmKikoSingleInCondition>(optShots[0])->getValues();
        Array temp = ext::dynamic_pointer_cast<FdmKikoSingleInCondition>(optShots[1])->getValues();
        Array back =
            ext::dynamic_pointer_cast<FdmKikoSingleInCondition>(optShots.back())->getValues();

        std::vector<Real> temp0, temp1, temp2;
        for (Size i = 0; i < snapValues.size(); ++i) {
            temp0.push_back(snapValues[i]);
            temp1.push_back(temp[i]);
            temp2.push_back(back[i]);
        }

        results_.delta = vanillaSolver->deltaAt(spot);

        results_.bucketedDeltaNPV10 = vanillaSolver->valueAt(spot / 1.0001);

        results_.gamma = vanillaSolver->gammaAt(spot);
        results_.theta = vanillaSolver->thetaAt(spot);
    }

    FdMurexKikoDoubleInEngine::FdMurexKikoDoubleInEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process,
        Size tGrid,
        Size xGrid,
        Size dampingSteps,
        const FdmSchemeDesc& schemeDesc,
        bool isPaymentAtMaturity,
        bool isBucketedDeltaAtMaturity,
        bool localVol,
        Real illegalLocalVolOverwrite)
    : process_(std::move(process)), // explicitDividends_(false),
      tGrid_(tGrid), xGrid_(xGrid), dampingSteps_(dampingSteps), schemeDesc_(schemeDesc),
      isPaymentAtMaturity_(isPaymentAtMaturity),
      isBucketedDeltaAtMaturity_(isBucketedDeltaAtMaturity), localVol_(localVol),
      illegalLocalVolOverwrite_(illegalLocalVolOverwrite) {

        registerWith(process_);
    }

    std::vector<Time>
    FdMurexKikoDoubleInEngine::BarrierTimes(const ext::shared_ptr<BarrierBase>& barrier,
                                            const Date& referenceDate) const {
        MonitoringType monitoring = barrier->monitoringType();

        Real rebate = barrier->rebate();

        std::vector<Date> discreteDates;

        std::vector<Time> barrierTimes;

        if (monitoring == MonitoringType::Discrete) {
            discreteDates = barrier->monitoringDates();
        }

        if (discreteDates.size() != 0) {
            for (auto& d : discreteDates) {
                barrierTimes.push_back(Actual365Fixed().yearFraction(referenceDate, d));
            }
        } else {
            Time start = Actual365Fixed().yearFraction(referenceDate, barrier->windowStart());
            Time end = Actual365Fixed().yearFraction(referenceDate, barrier->windowEnd());

            barrierTimes.push_back(start);

            // for continuous barrier, using daily barrier frequency
            for (Time t = start + 1.0 / 365.0; t < end; t += 1.0 / 365.0) {
                barrierTimes.push_back(t);
            }

            if (barrierTimes.back() != end) {
                barrierTimes.push_back(end);
            }
        }

        return barrierTimes;
        



    }

    void FdMurexKiKoDoubleInEngine::calculate() const {

        KIKOBarrierType kikoType = arguments_.kikobarrier_->KiKoType();
        FdmVanillaKiKoCondition::KIKOBarrierType vanillaKikoType =
            (kikoType == KIKOBarrierType::Any)   ? FdmVanillaKiKoCondition::KIKOBarrierType::Any :
            (kikoType == KIKOBarrierType::Until) ? FdmVanillaKiKoCondition::KIKOBarrierType::Until :
                                                   FdmVanillaKiKoCondition::KIKOBarrierType::After;

        QL_REQUIRE(kikoType == KIKOBarrierType::After, "KiKo type has to be After");

        double spot = process_->x0();

        double L = spot / 2.0;
        double U = spot * 2.0;


        double lower_in = QL_MIN_REAL;
        double upper_in = QL_MAX_REAL;


        double lower_in_upper_out = QL_MAX_REAL;
        double lower_in_lower_out = QL_MIN_REAL;

        double upper_in_upper_out = QL_MAX_REAL;
        double upper_in_lower_out = QL_MIN_REAL;

        // double lowerRebate = 0.0;
        double upperRebate = 0.0;
        */

            /*bool isOutOption = false;*/

            DoubleBarrierType InType =
            ext::dynamic_pointer_cast<DoubleBarrier>(arguments_.kikobarrier_->KnockInBarrier())
                ->type();

        QL_REQUIRE(InType == DoubleBarrierType::In, "KI type has to be In");

        // lower KO for lower KI

        SimpleBarrierType LowerOutType_Simple;
        DoubleBarrierType LowerOutType_Double;

        ext::shared_ptr<SimpleBarrier> LowerKnockOutBarrier_Simple =
            ext::dynamic_pointer_cast<SimpleBarrier>(
                arguments_.kikobarrier_->LowerKnockOutBarrier());

        ext::shared_ptr<DoubleBarrier> LowerKnockOutBarrier_Double =
            ext::dynamic_pointer_cast<DoubleBarrier>(
                arguments_.kikobarrier_->LowerKnockOutBarrier());

        bool is_LowerOutType_Simple = false;

        if (LowerKnockOutBarrier_Simple.get() != nullptr) {
            is_LowerOutType_Simple = true;

            LowerOutType_Simple = LowerKnockOutBarrier_Simple->type();

            QL_REQUIRE(((LowerOutType_Simple == SimpleBarrierType::DO) ||
                        (LowerOutType_Simple == SimpleBarrierType::UO)),
                       "Lower KO type has to be LO, UO, or Out");
        } else if (LowerKnockOutBarrier_Double.get() != nullptr) {
            LowerOutType_Double = LowerKnockOutBarrier_Double->type();

            QL_REQUIRE(LowerOutType_Double == DoubleBarrierType::Out,
                       "Lower KO type has to be LO, UO, or Out");
        } else {
            QL_FAIL("Lower KO invalid");
        }


        // upper KO for upper KI

        SimpleBarrierType UpperOutType_Simple;
        DoubleBarrierType UpperOutType_Double;

        ext::shared_ptr<SimpleBarrier> UpperKnockOutBarrier_Simple =
            ext::dynamic_pointer_cast<SimpleBarrier>(
                arguments_.kikobarrier_->UpperKnockOutBarrier());

        ext::shared_ptr<DoubleBarrier> UpperKnockOutBarrier_Double =
            ext::dynamic_pointer_cast<DoubleBarrier>(
                arguments_.kikobarrier_->UpperKnockOutBarrier());

        bool is_UpperOutType_Simple = false;

        if (UpperKnockOutBarrier_Simple.get() != nullptr) {
            is_UpperOutType_Simple = true;

            UpperOutType_Simple = UpperKnockOutBarrier_Simple->type();

            QL_REQUIRE(((UpperOutType_Simple == SimpleBarrierType::DO) ||
                        (UpperOutType_Simple == SimpleBarrierType::UO)),
                       "Upper KO type has to be LO, UO, or Out");
        } else if (UpperKnockOutBarrier_Double.get() != nullptr) {
            UpperOutType_Double = UpperKnockOutBarrier_Double->type();

            QL_REQUIRE(UpperOutType_Double == DoubleBarrierType::Out,
                       "Upper KO type has to be LO, UO, or Out");
        } else {
            QL_FAIL("Upper KO invalid");
        }


        ext::shared_ptr<DoubleBarrier> KnockInBarrier =
            ext::dynamic_pointer_cast<DoubleBarrier>(arguments_.kikobarrier_->KnockInBarrier());

        Date refDate = process_->riskFreeRate()->referenceDate();

        std::vector<Time> KnockInTimes = BarrierTimes(KnockInBarrier, refDate);


        std::vector<Time> LowerKnockOutTimes =
            is_LowerOutType_Simple ? BarrierTimes(LowerKnockOutBarrier_Simple, refDate) :
                                     BarrierTimes(LowerKnockOutBarrier_Double, refDate);

        std::vector<Time> UpperKnockOutTimes =
            is_UpperOutType_Simple ? BarrierTimes(UpperKnockOutBarrier_Simple, refDate) :
                                     BarrierTimes(UpperKnockOutBarrier_Double, refDate);


        // 1. Mesher

        const ext::shared_ptr<StrikedTypePayoff> lowerpayoff =
            ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.lowerpayoff_);

        const ext::shared_ptr<StrikedTypePayoff> upperpayoff =
            ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.upperpayoff_);

        const Time maturity = process_->time(arguments_.exercise->lastDate());

        if (KnockInBarrier->type() == DoubleBarrierType::In) {
            upper_in = KnockInBarrier->upperBarrier();
            lower_in = KnockInBarrier->lowerBarrier();
        }

        // lower KO barrier values
        if (is_LowerOutType_Simple) {

            if (LowerKnockOutBarrier_Simple->type() == SimpleBarrierType::UO) {
                lower_in_upper_out = LowerKnockOutBarrier_Simple->barrier();
            } else if (LowerKnockOutBarrier_Simple->type() == SimpleBarrierType::DO) {
                lower_in_lower_out = LowerKnockOutBarrier_Simple->barrier();
            } else {
                QL_FAIL("Lower KO Barrier Type should be DO or UO");
            }
        } else {

            lower_in_upper_out = LowerKnockOutBarrier_Double->upperBarrier();
            lower_in_lower_out = LowerKnockOutBarrier_Double->lowerBarrier();
        }


        // upper KO barrier values
        if (is_UpperOutType_Simple) {

            if (UpperKnockOutBarrier_Simple->type() == SimpleBarrierType::UO) {
                upper_in_upper_out = UpperKnockOutBarrier_Simple->barrier();
            } else if (UpperKnockOutBarrier_Simple->type() == SimpleBarrierType::DO) {
                upper_in_lower_out = UpperKnockOutBarrier_Simple->barrier();
            } else {
                QL_FAIL("Upper KO Barrier Type should be DO or UO");
            }
        } else {

            upper_in_upper_out = UpperKnockOutBarrier_Double->upperBarrier();
            upper_in_lower_out = UpperKnockOutBarrier_Double->lowerBarrier();
        }

        /////////////////////////////////////////////////////////////////////////////

        // in mesher
        std::vector<Real> mesherPoints;
        for (Size i = 0; i < xGrid_; ++i) {
            mesherPoints.push_back(std::log(L + i * (U - L) / xGrid_));
        }

        if (upper_in != QL_MAX_REAL) {
            mesherPoints.push_back(std::log(upper_in));
        }

        if (lower_in != QL_MIN_REAL) {
            mesherPoints.push_back(std::log(lower_in));
        }

        if (lower_in_upper_out != QL_MAX_REAL) {
            mesherPoints.push_back(std::log(lower_in_upper_out));
        }

        if (lower_in_lower_out != QL_MIN_REAL) {
            mesherPoints.push_back(std::log(lower_in_lower_out));
        }

        if (upper_in_upper_out != QL_MAX_REAL) {
            mesherPoints.push_back(std::log(upper_in_upper_out));
        }

        if (upper_in_lower_out != QL_MIN_REAL) {
            mesherPoints.push_back(std::log(upper_in_lower_out));
        }


        std::sort(mesherPoints.begin(), mesherPoints.end());
        mesherPoints.erase(std::unique(mesherPoints.begin(), mesherPoints.end()),
                           mesherPoints.end());

        ext::shared_ptr<FdmMesher> InMesher = ext::make_shared<PredefinedMesher>(mesherPoints);

        /////////////////////////////////////////////////////////////////////////////

        // determine if barriers are active

        bool isKIActive = false, isLowerKOActive = false, isUpperKOActive = false;

        /////////////////////////////////////////////////////////////////////////////

        std::vector<Time> monitoringTimes;
        // monitoringTimes.reserve(KnockInTimes.size() + LowerKnockOutTimes.size() +
        // UpperKnockOutTimes.size() + 2); // preallocate memory

        // monitoringTimes.push_back(0.0);
        monitoringTimes.push_back(maturity);

        monitoringTimes.insert(monitoringTimes.end(), KnockInTimes.begin(), KnockInTimes.end());
        monitoringTimes.insert(monitoringTimes.end(), LowerKnockOutTimes.begin(),
                               LowerKnockOutTimes.end());
        monitoringTimes.insert(monitoringTimes.end(), UpperKnockOutTimes.begin(),
                               UpperKnockOutTimes.end());

        std::sort(monitoringTimes.begin(), monitoringTimes.end());
        monitoringTimes.erase(std::unique(monitoringTimes.begin(), monitoringTimes.end()),
                              monitoringTimes.end());

        /////////////////////////////////////////////////////////////////////////////

        // 2. Calculator


        const ext::shared_ptr<FdmInnerValueCalculator> lowerInCalculator(
            new FdmLogInnerValue(lowerpayoff, KnockInMesher, 0));

        const ext::shared_ptr<FdmInnerValueCalculator> upperInCalculator(
            new FdmLogInnerValue(upperpayoff, KnockInMesher, 0));

        // 3. Boundary conditions
        FdmBoundaryConditionSet boundaries;

        // 4. Step conditions
        QL_REQUIRE(arguments_.exercise->type() == Exercise::European,
                   "only european style option are supported");

        Date referenceDate = process_->riskFreeRate()->referenceDate();

        // insert barrier condition

        std::list<ext::shared_ptr<StepCondition<Array>>> stepConditions;
        std::list<std::vector<Time>> stoppingTimes;

        // stoppingTimes.emplace_back(KnockOutTimes);
        // stoppingTimes.emplace_back(KnockInTimes);
        stoppingTimes.emplace_back(monitoringTimes);

        // std::list<ext::shared_ptr<StepCondition<Array>>> translationOptConditions;

        std::list<ext::shared_ptr<StepCondition<Array>>> lowerInVanillaOptConditions;
        std::list<ext::shared_ptr<StepCondition<Array>>> upperInVanillaOptConditions;
        /* solve PDE for vanilla option */

        std::vector<ext::shared_ptr<StepCondition<Array>>> snapShotsLowerIn;
        std::vector<ext::shared_ptr<StepCondition<Array>>> snapShotsUpperIn;

        // using translationOptConditions to snapshot vanilla prices at each monitoring dates
        for (Size i = 0; i < monitoringTimes.size(); ++i) {

            // lower in
            snapShotsLowerIn.push_back(
                ext::shared_ptr<StepCondition<Array>>(new FdmVanillaKiKoCondition(
                    monitoringTimes[i], maturity, KnockInMesher, lower_in_lower_out,
                    lower_in_upper_out, 0.0, 0.0, lowerpayoff, vanillaKikoType,
                    i == monitoringTimes.size() - 1 ? true : false, // is maturity
                    isBucketedDeltaAtMaturity_,
                    std::find_if(KnockInTimes.begin(), KnockInTimes.end(),
                                 [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                        KnockInTimes.end(), // is KI active
                    std::find_if(LowerKnockOutTimes.begin(), LowerKnockOutTimes.end(),
                                 [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                        LowerKnockOutTimes.end() // is lower KO active
                    )));

            lowerInVanillaOptConditions.push_back(snapShotsLowerIn[i]);

            // upper in
            snapShotsUpperIn.push_back(
                ext::shared_ptr<StepCondition<Array>>(new FdmVanillaKiKoCondition(
                    monitoringTimes[i], maturity, KnockInMesher, upper_in_lower_out,
                    upper_in_upper_out, 0.0, 0.0, upperpayoff, vanillaKikoType,
                    i == monitoringTimes.size() - 1 ? true : false, isBucketedDeltaAtMaturity_,
                    std::find_if(KnockInTimes.begin(), KnockInTimes.end(),
                                 [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                        KnockInTimes.end(), // is KI active
                    std::find_if(UpperKnockOutTimes.begin(), UpperKnockOutTimes.end(),
                                 [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                        UpperKnockOutTimes.end() // is upper KO active
                    )));

            upperInVanillaOptConditions.push_back(snapShotsUpperIn[i]);
        }


        ext::shared_ptr<FdmStepConditionComposite> lowerInConditionsSet(
            new FdmStepConditionComposite(stoppingTimes, lowerInVanillaOptConditions));

        ext::shared_ptr<FdmStepConditionComposite> upperInConditionsSet(
            new FdmStepConditionComposite(stoppingTimes, upperInVanillaOptConditions));


        FdmSolverDesc lowerInVanillaSolverDesc = {
            KnockInMesher, boundaries, lowerInConditionsSet, lowerInCalculator,
            maturity,      tGrid_,     dampingSteps_};

        FdmSolverDesc upperInVanillaSolverDesc = {
            KnockInMesher, boundaries, upperInConditionsSet, upperInCalculator,
            maturity,      tGrid_,     dampingSteps_};

        const ext::shared_ptr<FdmBlackScholesSolver> lowerInVanillaSolver(
            ext::make_shared<FdmBlackScholesSolver>(
                Handle<GeneralizedBlackScholesProcess>(process_), lowerpayoff->strike(),
                lowerInVanillaSolverDesc, schemeDesc_, localVol_, illegalLocalVolOverwrite_));

        const ext::shared_ptr<FdmBlackScholesSolver> upperInVanillaSolver(
            ext::make_shared<FdmBlackScholesSolver>(
                Handle<GeneralizedBlackScholesProcess>(process_), upperpayoff->strike(),
                upperInVanillaSolverDesc, schemeDesc_, localVol_, illegalLocalVolOverwrite_));


        // all snap shots completed this stage
        double lowerInVanillaPV = lowerInVanillaSolver->valueAt(spot);
        double upperInVanillaPV = upperInVanillaSolver->valueAt(spot);


        std::list < ext::shared_ptr < StepCondition < Array >>>> optConditions;


        for (Size i = 0; i < monitoringTimes.size(); ++i) {

            // QL_REQUIRE(KnockInTimes[i] == LowerKnockOutTimes[i], "Only same KnockIn/Out times are
            // supported"); QL_REQUIRE(KnockInTimes[i] == UpperKnockOutTimes[i], "Only same
            // KnockIn/Out times are supported");

            Array lowerInSnapValues =
                ext::dynamic_pointer_cast<FdmVanillaKiKoCondition>(snapShotsLowerIn[i])
                    ->getValues();
            Array upperInSnapValues =
                ext::dynamic_pointer_cast<FdmVanillaKiKoCondition>(snapShotsUpperIn[i])
                    ->getValues();

            optConditions.push_back(
                ext::shared_ptr<StepCondition<Array>>(new FdmKiKoDoubleInCondition(
                    monitoringTimes[i], KnockInMesher, lower_in, upper_in,
                    arguments_.kikobarrier_->KiKoType(), lowerInSnapValues, upperInSnapValues,
                    i == KnockInTimes.size() - 1 ? true : false, isBucketedDeltaAtMaturity_,
                    std::find_if(KnockInTimes.begin(), KnockInTimes.end(),
                                 [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                        KnockInTimes.end(), // is lower KO active
                    std::find_if(LowerKnockOutTimes.begin(), LowerKnockOutTimes.end(),
                                 [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                        LowerKnockOutTimes.end(),
                    std::find_if(UpperKnockOutTimes.begin(), UpperKnockOutTimes.end(),
                                 [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                        UpperKnockOutTimes.end(), // is upper KO active
                    isPaymentAtMaturity_)));
        }


        ext::shared_ptr<FdmStepConditionComposite> optConditionsSet(
            new FdmStepConditionComposite(stoppingTimes, optConditions));

        // 5. Solver
        FdmSolverDesc solverDesc = {KnockInMesher, boundaries, optConditionsSet, lowerInCalculator,
                                    maturity,      tGrid_,     dampingSteps_};

        const ext::shared_ptr<FdmBlackScholesSolver> solver(new FdmBlackScholesSolver(
            Handle<GeneralizedBlackScholesProcess>(process_), lowerpayoff->strike(), solverDesc,
            schemeDesc_, localVol_, illegalLocalVolOverwrite_));

        results_.value = solver->valueAt(spot);
        results_.delta = solver->deltaAt(spot);

        results_.bucketedDeltaMPVTO = solver->valueAt(spot / 1.0002);

        results_.gamma = solver->gammaAt(spot);
        results_.theta = solver->thetaAt(spot);
    }





}


