/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
Copyright (C) 2003, 2004 Neil Firth
Copyright (C) 2003, 2004 Ferdinando Ametrano
Copyright (C) 2003, 2004, 2005, 2007, 2008 StatPro Italia srl

This file is part of QuantLib, a free-software/open-source library
for financial quantitative analysts and developers - http://quantlib.org/

QuantLib is free software: you can redistribute it and/or modify it
under the terms of the QuantLib license. You should have received a
copy of the license along with this program; if not, please email
<quantlib-dev@lists.sf.net>. The license is also available online at
<http://quantlib.org/license.shtml>.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file mcbarrierengine.hpp
    \brief Monte Carlo barrier option engines

    self-developed by:
        Yue Zhang
*/

#ifndef quantlib_mc_barrier_engines_hpp
#define quantlib_mc_barrier_engines_hpp

#include <ql/exercise.hpp>
#include <ql/instruments/barriers/barrieroption.hpp>
#include <ql/pricingengines/mcsimulation.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <utility>

namespace QuantLib {

    //! Pricing engine for barrier options using Monte Carlo simulation
    /*! Uses the Brownian-bridge correction for the barrier found in
        <i>
        Going to Extremes: Correcting Simulation Bias in Exotic
        Option Valuation - D.R. Beaglehole, P.H. Dybvig and G. Zhou
        Financial Analysts Journal; Jan/Feb 1997; 53, 1. pg. 62-68
        </i>
        and
        <i>
        Simulating path-dependent options: A new approach -
        M. El Babsiri and G. Noel
        Journal of Derivatives; Winter 1998; 6, 2; pg. 65-83
        </i>

        \ingroup barrierengines

        \test the correctness of the returned value is tested by
              reproducing results available in literature.
    */
    template <class RNG = PseudoRandom, class S = Statistics>
    class MCSimpleBarrierEngine : public BarrierOption::engine,
                                  public McSimulation<SingleVariate, RNG, S> {
      public:
        typedef
            typename McSimulation<SingleVariate, RNG, S>::path_generator_type path_generator_type;
        typedef typename McSimulation<SingleVariate, RNG, S>::path_pricer_type path_pricer_type;
        typedef typename McSimulation<SingleVariate, RNG, S>::stats_type stats_type;

        // constructor
        MCSimpleBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                              Size timeSteps,
                              Size timeStepsPerYear,
                              bool brownianBridge,
                              bool antitheticVariate,
                              Size requiredSamples,
                              Real requiredTolerance,
                              Size maxSamples,
                              bool isBiased,
                              BigNatural seed);

        void calculate() const override {

            Real spot = process_->x0();
            QL_REQUIRE(arguments_.barrierBase, "simple barrier not given");
            QL_REQUIRE(spot > 0.0, "negative or null underlying given");

            // QL_REQUIRE(!triggered(spot), "barrier touched");
            McSimulation<SingleVariate, RNG, S>::calculate(requiredTolerance_, requiredSamples_,
                                                           maxSamples_);

            results_.value = this->mcModel_->sampleAccumulator().mean();

            if (RNG::allowsErrorEstimate)
                results_.errorEstimate = this->mcModel_->sampleAccumulator().errorEstimate();
        }

      protected:
        // McSimulation implementation
        TimeGrid timeGrid() const override;
        ext::shared_ptr<path_generator_type> pathGenerator() const override {

            TimeGrid grid = timeGrid();

            typename RNG::rsg_type gen = RNG::make_sequence_generator(grid.size() - 1, seed_);

            return ext::shared_ptr<path_generator_type>(
                new path_generator_type(process_, grid, gen, brownianBridge_));
        }

        ext::shared_ptr<path_pricer_type> pathPricer() const override;

        // data members
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        Size timeSteps_, timeStepsPerYear_;
        Size requiredSamples_, maxSamples_;
        Real requiredTolerance_;
        bool isBiased_;
        bool brownianBridge_;
        BigNatural seed_;
    };


    //! Monte Carlo barrier-option engine factory
    template <class RNG = PseudoRandom, class S = Statistics>
    class MakeMCSimpleBarrierEngine {
      public:
        MakeMCSimpleBarrierEngine(ext::shared_ptr<GeneralizedBlackScholesProcess>);
        // named parameters
        MakeMCSimpleBarrierEngine& withSteps(Size steps);
        MakeMCSimpleBarrierEngine& withStepsPerYear(Size steps);
        MakeMCSimpleBarrierEngine& withBrownianBridge(bool b = true);
        MakeMCSimpleBarrierEngine& withAntitheticVariate(bool b = true);
        MakeMCSimpleBarrierEngine& withSamples(Size samples);
        MakeMCSimpleBarrierEngine& withAbsoluteTolerance(Real tolerance);
        MakeMCSimpleBarrierEngine& withMaxSamples(Size samples);
        MakeMCSimpleBarrierEngine& withBias(bool b = true);
        MakeMCSimpleBarrierEngine& withSeed(BigNatural seed);
        // conversion to pricing engine
        operator ext::shared_ptr<PricingEngine>() const;

      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        bool brownianBridge_ = false, antithetic_ = false, biased_ = false;
        Size steps_, stepsPerYear_, samples_, maxSamples_;
        Real tolerance_;
        BigNatural seed_ = 0;
    };


    class SimpleBarrierPathPricer : public PathPricer<Path> {
      public:
        SimpleBarrierPathPricer(const ext::shared_ptr<BarrierBase> barrier,
                                Option::Type type,
                                Real strike,
                                std::vector<DiscountFactor> discounts,
                                ext::shared_ptr<StochasticProcess1D> diffProcess,
                                Size monitoringBegin,
                                Size monitoringEnd);

        Real operator()(const Path& path) const override;

      private:
        SimpleBarrierType barrierType_;
        Real barrier_;
        Real rebate_;
        ext::shared_ptr<StochasticProcess1D> diffProcess_;
        // PseudoRandom::ursg_type sequenceGen_;
        PlainVanillaPayoff payoff_;
        std::vector<DiscountFactor> discounts_;

      private:
        ext::shared_ptr<BarrierBase> Barrier_;
        Size monitoringBegin_;
        Size monitoringEnd_;
    };

    // template definitions

    template <class RNG, class S>
    inline MCSimpleBarrierEngine<RNG, S>::MCSimpleBarrierEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process,
        Size timeSteps,
        Size timeStepsPerYear,
        bool brownianBridge,
        bool antitheticVariate,
        Size requiredSamples,
        Real requiredTolerance,
        Size maxSamples,
        bool isBiased,
        BigNatural seed)
    : McSimulation<SingleVariate, RNG, S>(antitheticVariate, false), process_(std::move(process)),
      timeSteps_(timeSteps), timeStepsPerYear_(timeStepsPerYear), requiredSamples_(requiredSamples),
      maxSamples_(maxSamples), requiredTolerance_(requiredTolerance), isBiased_(isBiased),
      brownianBridge_(brownianBridge), seed_(seed) {
        QL_REQUIRE(timeSteps_ != Null<Size>() || timeStepsPerYear_ != Null<Size>(),
                   "no time steps provided");
        QL_REQUIRE(timeSteps_ == Null<Size>() || timeStepsPerYear_ == Null<Size>(),
                   "both time steps and time steps per year were provided");
        QL_REQUIRE(timeSteps_ != 0, "timeSteps must be positive, " << timeSteps_ << " not allowed");
        QL_REQUIRE(timeStepsPerYear_ != 0,
                   "timeStepsPerYear must be positive, " << timeStepsPerYear_ << " not allowed");
        registerWith(process_);
    }

    template <class RNG, class S>
    inline TimeGrid MCSimpleBarrierEngine<RNG, S>::timeGrid() const {

        std::vector<Time> monitoringTimes;

        const MonitoringType mType = arguments_.barrierBase->monitoringType();

        if (mType == MonitoringType::Continuous) {

            double startTime = process_->time(arguments_.barrierBase->windowStart());
            double endTime = process_->time(arguments_.barrierBase->windowEnd());

            std::vector<Time> mTimes{startTime, endTime};

            double dt = 1.0 / 365.0 / 4;

            TimeGrid times(mTimes.begin(), mTimes.end(),
                           static_cast<Size>((endTime - startTime) / dt));

            monitoringTimes = (startTime == endTime) ?
                                  std::vector<Time>(1, startTime) :
                                  std::vector<Time>(times.begin(), times.end());
        } else {
            std::vector<Date> mDates = arguments_.barrierBase->monitoringDates();

            monitoringTimes.resize(mDates.size());

            std::transform(mDates.begin(), mDates.end(), monitoringTimes.begin(),
                           [&](Date d) { return process_->time(d); });
        }

        Time residualTime = process_->time(arguments_.exercise->lastDate());

        monitoringTimes.push_back(residualTime);

        if (timeSteps_ != Null<Size>()) {
            return TimeGrid(monitoringTimes.begin(), monitoringTimes.end(), timeSteps_);
        } else if (timeStepsPerYear_ != Null<Size>()) {
            Size steps = static_cast<Size>(timeStepsPerYear_ * residualTime);
            return TimeGrid(monitoringTimes.begin(), monitoringTimes.end(),
                            std::max<Size>(steps, 1));
        } else {
            QL_FAIL("time steps not specified");
        }
    }

    template <class RNG, class S>
    inline ext::shared_ptr<typename MCSimpleBarrierEngine<RNG, S>::path_pricer_type>
    MCSimpleBarrierEngine<RNG, S>::pathPricer() const {
        ext::shared_ptr<PlainVanillaPayoff> payoff =
            ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");

        TimeGrid grid = timeGrid();
        std::vector<DiscountFactor> discounts(grid.size());
        for (Size i = 0; i < grid.size(); i++)
            discounts[i] = process_->riskFreeRate()->discount(grid[i]);

        // find iterators
        Size mBegin = grid.closestIndex(process_->time(arguments_.barrierBase->windowStart()));
        Size mEnd = grid.closestIndex(process_->time(arguments_.barrierBase->windowEnd()));

        return ext::shared_ptr<typename MCSimpleBarrierEngine<RNG, S>::path_pricer_type>(
            /*
            new SimpleBarrierPathPricer(
                arguments_.simplebarrier->type(),
                arguments_.simplebarrier->barrier(),
                arguments_.simplebarrier->rebate(),
                payoff->optionType(),
                payoff->strike(),
                discounts,
                process_, mBegin, mEnd)*/
            new SimpleBarrierPathPricer(arguments_.barrierBase, payoff->optionType(),
                                        payoff->strike(), discounts, process_, mBegin, mEnd));
    }


    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>::MakeMCSimpleBarrierEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process)), steps_(Null<Size>()), stepsPerYear_(Null<Size>()),
      samples_(Null<Size>()), maxSamples_(Null<Size>()), tolerance_(Null<Real>()) {}

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>&
    MakeMCSimpleBarrierEngine<RNG, S>::withSteps(Size steps) {
        steps_ = steps;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>&
    MakeMCSimpleBarrierEngine<RNG, S>::withStepsPerYear(Size steps) {
        stepsPerYear_ = steps;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>&
    MakeMCSimpleBarrierEngine<RNG, S>::withBrownianBridge(bool brownianBridge) {
        brownianBridge_ = brownianBridge;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>&
    MakeMCSimpleBarrierEngine<RNG, S>::withAntitheticVariate(bool b) {
        antithetic_ = b;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>&
    MakeMCSimpleBarrierEngine<RNG, S>::withSamples(Size samples) {
        QL_REQUIRE(tolerance_ == Null<Real>(), "tolerance already set");
        samples_ = samples;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>&
    MakeMCSimpleBarrierEngine<RNG, S>::withAbsoluteTolerance(Real tolerance) {
        QL_REQUIRE(samples_ == Null<Size>(), "number of samples already set");
        QL_REQUIRE(RNG::allowsErrorEstimate, "chosen random generator policy "
                                             "does not allow an error estimate");
        tolerance_ = tolerance;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>&
    MakeMCSimpleBarrierEngine<RNG, S>::withMaxSamples(Size samples) {
        maxSamples_ = samples;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>&
    MakeMCSimpleBarrierEngine<RNG, S>::withBias(bool biased) {
        biased_ = biased;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>&
    MakeMCSimpleBarrierEngine<RNG, S>::withSeed(BigNatural seed) {
        seed_ = seed;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCSimpleBarrierEngine<RNG, S>::operator ext::shared_ptr<PricingEngine>() const {
        QL_REQUIRE(steps_ != Null<Size>() || stepsPerYear_ != Null<Size>(),
                   "number of steps not given");
        QL_REQUIRE(steps_ == Null<Size>() || stepsPerYear_ == Null<Size>(),
                   "number of steps overspecified");
        return ext::shared_ptr<PricingEngine>(new MCSimpleBarrierEngine<RNG, S>(
            process_, steps_, stepsPerYear_, brownianBridge_, antithetic_, samples_, tolerance_,
            maxSamples_, biased_, seed_));
    }

}

#endif