#include "fdmurexbarrierengine.hpp"
#include <ql/instruments/vanillaoption.hpp>
#include <ql/methods/finitedifferences/stepconditions/fdmdoublebarriersstepcondition.h>
#include <ql/methods/finitedifferences/stepconditions/fdmvanilladoublebarriersstepcondition.h>
#include <ql/pricingengines/vanilla/fdblackscholesvanillaengine.hpp>
#include <list>
#include <utility>
#include <ql/instruments/barriers/barriers.h>

namespace QuantLib {

    FdMurexBarrierEngine::FdMurexBarrierEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process,
        Size tGrid,
        Size xGrid,
        Size dampingSteps,
        const FdmSchemeDesc& schemeDesc,
        bool isBucketedDeltaAtMaturity,
        bool localVol,
        Real illegalLocalVolOverwrite)
    : process_(std::move(process)), // explicitDividends_(false),
      tGrid_(tGrid), xGrid_(xGrid), dampingSteps_(dampingSteps), schemeDesc_(schemeDesc),
      isBucketedDeltaAtMaturity_(isBucketedDeltaAtMaturity), localVol_(localVol),
      illegalLocalVolOverwrite_(illegalLocalVolOverwrite) {

        registerWith(process_);
    }

    void FdMurexBarrierEngine::calculate() const {

        double spot = process_->x0();

        double L = spot / 2.0;
        double U = spot * 2.0;

        double lower = QL_MIN_REAL;
        double upper = QL_MAX_REAL;

        double lowerRebate = 0.0;
        double upperRebate = 0.0;

        bool isOutOption = false;
        
        if (ext::dynamic_pointer_cast<SimpleBarrier>(arguments_.barrierBase)) {
            // if simple barrier

            SimpleBarrierType type =
                ext::dynamic_pointer_cast<SimpleBarrier>(arguments_.barrierBase)->type();

            if (type == SimpleBarrierType::UI || type == SimpleBarrierType::UO) {
                // U = std::dynamic_pointer_cast<SimpleBarrier>(arguments_.barrierBase)->barrier();
                // upper = U;

                upper = ext::dynamic_pointer_cast<SimpleBarrier>(arguments_.barrierBase)->barrier();
                upperRebate =
                    ext::dynamic_pointer_cast<SimpleBarrier>(arguments_.barrierBase)->rebate();
            } else {
                // L = std::dynamic_pointer_cast<SimpleBarrier>(arguments_.barrierBase)->barrier();
                // lower = L;

                lower = ext::dynamic_pointer_cast<SimpleBarrier>(arguments_.barrierBase)->barrier();
                lowerRebate =
                    ext::dynamic_pointer_cast<SimpleBarrier>(arguments_.barrierBase)->rebate();
            }

            if (type == SimpleBarrierType::UO || type == SimpleBarrierType::DO) {
                // fdmBarrierType = FdmDoubleBarrierCondition::KnockType::Out;

                isOutOption = true;
            }
        } else {
            // double barrier

            DoubleBarrierType type =
                ext::dynamic_pointer_cast<DoubleBarrier>(arguments_.barrierBase)->type();

            // U = std::dynamic_pointer_cast<DoubleBarrier>(arguments_.barrierBase)->upperBarrier();
            // upper = U;

            // L = std::dynamic_pointer_cast<DoubleBarrier>(arguments_.barrierBase)->lowerBarrier();
            // lower = L;

            upper =
                ext::dynamic_pointer_cast<DoubleBarrier>(arguments_.barrierBase)->upperBarrier();
            lower =
                ext::dynamic_pointer_cast<DoubleBarrier>(arguments_.barrierBase)->lowerBarrier();

            upperRebate = lowerRebate =
                ext::dynamic_pointer_cast<DoubleBarrier>(arguments_.barrierBase)->rebate();

            if (type == DoubleBarrierType::Out) {
                // fdmBarrierType = FdmDoubleBarrierCondition::KnockType::Out;

                isOutOption = true;
            }
        }

        MonitoringType monitoring = arguments_.barrierBase->monitoringType();

        // concentration points for mesher
        std::vector<std::tuple<Real, Real, bool>> concentrationPoints;

        if (upper != QL_MAX_REAL) {
            concentrationPoints.push_back({std::log(upper), 0.01, true});
        }

        if (lower != QL_MIN_REAL) {
            concentrationPoints.push_back({std::log(lower), 0.01, true});
        }

        // 1. Mesher
        const ext::shared_ptr<StrikedTypePayoff> payoff =
            ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        const Time maturity = process_->time(arguments_.exercise->lastDate());

        ext::shared_ptr<Fdm1dMesher> equityMesher;

        std::vector<Real> mesherPoints;

        // if (monitoring == MonitoringType::Continuous)
        //{
        //     if (isOutOption) {
        //         // 1.1. knock-out option;
        //         // The barrier is select as [L,U]
        //         // for single barrier, L = spot/1.3 or U = spot * 1.3
        //
        //         equityMesher
        //         = ext::make_shared<UniformldMesher>(
        //             std::log(std::max(L, lower)) - 1e-8,
        //             std::log(std::min(U, upper)) + 1e-8,
        //             xGrid_
        //         );
        //     }

        for (Size i = 0; i < xGrid_; ++i) {
            mesherPoints.push_back(std::log(L + i * (U - L) / xGrid_));
        }

        if (upper != QL_MAX_REAL) {
            mesherPoints.push_back(std::log(upper));
        }

        if (lower != QL_MIN_REAL) {
            mesherPoints.push_back(std::log(lower));
        }

        std::sort(mesherPoints.begin(), mesherPoints.end());
        mesherPoints.erase(std::unique(mesherPoints.begin(), mesherPoints.end()),
                           mesherPoints.end());

        equityMesher = ext::make_shared<Predefined1dMesher>(mesherPoints);

        const ext::shared_ptr<FdmMesher> mesher(new FdmMesherComposite(equityMesher));



        // 2. Calculator

        const ext::shared_ptr<FdmInnerValueCalculator> calculator(
            new FdmLogInnerValue(payoff, mesher, 0));

        // 3. Boundary conditions
        FdmBoundaryConditionSet boundaries;

        // 4. Step conditions
        QL_REQUIRE(arguments_.exercise->type() == Exercise::European,
                   "only european style option are supported");

        Date referenceDate = process_->riskFreeRate()->referenceDate();

        // insert barrier condition

        double rebate = arguments_.barrierBase->rebate();

        std::vector<Date> discreteDates;

        if (monitoring == MonitoringType::Discrete) {
            discreteDates = arguments_.barrierBase->monitoringDates();
        }

        std::vector<Time> barrierTimes;
        std::vector<Time> monitoringTimes;

        if (discreteDates.size() != 0) {

            for (auto& d : discreteDates) {
                barrierTimes.push_back(Actual365Fixed().yearFraction(referenceDate, d));
            }
        } else {
            Time start =
                Actual365Fixed().yearFraction(referenceDate, arguments_.barrierBase->windowStart());
            Time end =
                Actual365Fixed().yearFraction(referenceDate, arguments_.barrierBase->windowEnd());

            barrierTimes.push_back(start);

            // for continuous barrier, using daily barrier frequency
            Real timegap = 1.0;
            for (Time t = start + 1.0 / 365.0; t < end; t += timegap / 365.0) {
                barrierTimes.push_back(t);
            }

            if (barrierTimes.back() != end) {
                barrierTimes.push_back(end);
            }
        }

        monitoringTimes.insert(monitoringTimes.end(), barrierTimes.begin(), barrierTimes.end());
        monitoringTimes.push_back(maturity);
        std::sort(monitoringTimes.begin(), monitoringTimes.end());
        monitoringTimes.erase(std::unique(monitoringTimes.begin(), monitoringTimes.end()),
                              monitoringTimes.end());

        FdmDoubleBarrierCondition::KnockType fdmBarrierType =
            isOutOption ? FdmDoubleBarrierCondition::KnockType::Out :
                          FdmDoubleBarrierCondition::KnockType::In;

        FdmVanillaDoubleBarrierCondition::KnockType fdmVanillaBarrierType =
            isOutOption ? FdmVanillaDoubleBarrierCondition::KnockType::Out :
                          FdmVanillaDoubleBarrierCondition::KnockType::In;

        std::list<std::vector<Time>> stoppingTimes;

        stoppingTimes.emplace_back(monitoringTimes);

        std::list<ext::shared_ptr<StepCondition<Array>>> vanillaConditions;


        std::vector<ext::shared_ptr<StepCondition<Array>>> vanillaSnapShots;

        // if (isOutOption)

        // using snap conditions to snapshot vanilla prices at each monitoring dates

        for (Size i = 0; i < monitoringTimes.size(); ++i) {

            /*snapShots.push_back(ext::shared_ptr<StepCondition<Array>>(
                            new FdmSnapshotCondition(barrierTimes[i])
                    ));*/

            vanillaSnapShots.push_back(
                ext::shared_ptr<StepCondition<Array>>(new FdmVanillaDoubleBarrierCondition(
                    monitoringTimes[i], mesher, lower, upper, lowerRebate, upperRebate,
                    std::find_if(barrierTimes.begin(), barrierTimes.end(),
                                 [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                        barrierTimes.end(),
                    fdmVanillaBarrierType, 
                    (i == monitoringTimes.size() - 1) ? true : false, isBucketedDeltaAtMaturity_,
                    payoff)));

            vanillaConditions.push_back(vanillaSnapShots[i]);
        }

        ext::shared_ptr<FdmStepConditionComposite> snapshotConditions(
            new FdmStepConditionComposite(stoppingTimes, vanillaConditions));

        FdmSolverDesc vanillaSolverDesc = {mesher,   boundaries, snapshotConditions, calculator,
                                           maturity, tGrid_,     dampingSteps_};

        const ext::shared_ptr<FdmBlackScholesSolver> vanillaSolver(
            ext::make_shared<FdmBlackScholesSolver>(
                Handle<GeneralizedBlackScholesProcess>(process_), payoff->strike(),
                vanillaSolverDesc, schemeDesc_, localVol_, illegalLocalVolOverwrite_));

        // all snap shots completed this stage
        double vanillaPV = vanillaSolver->valueAt(spot);

        std::list<ext::shared_ptr<StepCondition<Array>>> snapvectors(vanillaSnapShots.begin(),
                                                                     vanillaSnapShots.end());

        std::vector<ext::shared_ptr<StepCondition<Array>>> opSnapShots;

        for (Size i = 0; i < monitoringTimes.size(); ++i) {

            Array snapValues =
                ext::dynamic_pointer_cast<FdmVanillaDoubleBarrierCondition>(vanillaSnapShots[i])
                    ->getValues();
            

            opSnapShots.push_back(
                ext::shared_ptr<StepCondition<Array>>(new FdmDoubleBarrierCondition(
                    monitoringTimes[i], mesher, lower, upper, lowerRebate, upperRebate,
                    std::find_if(barrierTimes.begin(), barrierTimes.end(),
                                 [&](Time t) { return abs(t - monitoringTimes.at(i)) < 1e-8; }) !=
                        barrierTimes.end(),
                    fdmBarrierType, snapValues, (i == monitoringTimes.size() - 1) ? true : false,
                    isBucketedDeltaAtMaturity_, payoff)));

            snapvectors.push_back(opSnapShots[i]);
        }

        ext::shared_ptr<FdmStepConditionComposite> opstepConditions(
            new FdmStepConditionComposite(stoppingTimes, snapvectors));

        // 5. Solver
        FdmSolverDesc solverDesc = {mesher,   boundaries, opstepConditions, calculator,
                                    maturity, tGrid_,     dampingSteps_};

        const ext::shared_ptr<FdmBlackScholesSolver> solver(new FdmBlackScholesSolver(
            Handle<GeneralizedBlackScholesProcess>(process_), payoff->strike(), solverDesc,
            schemeDesc_, localVol_, illegalLocalVolOverwrite_));

        results_.value = solver->valueAt(spot);

        std::vector<ext::shared_ptr<StepCondition<Array>>> opSnapvectors(opSnapShots.begin(),
                                                                         opSnapShots.end());

        Array snapValues =
            ext::dynamic_pointer_cast<FdmDoubleBarrierCondition>(opSnapvectors[0])->getValues();
        Array temp =
            ext::dynamic_pointer_cast<FdmDoubleBarrierCondition>(opSnapvectors[1])->getValues();
        Array back =
            ext::dynamic_pointer_cast<FdmDoubleBarrierCondition>(opSnapvectors.back())->getValues();

        std::vector<Real> temp0, temp1, temp2;
        for (Size i = 0; i < snapValues.size(); ++i) {
            temp0.push_back(snapValues[i]);
            temp1.push_back(temp[i]);
            temp2.push_back(back[i]);
        }

        //results_.bucketedDeltaMPVTO = solver->valueAt(spot / 1.0001);

        results_.delta = solver->deltaAt(spot);
        results_.gamma = solver->gammaAt(spot);
        results_.theta = solver->thetaAt(spot);
    }

}