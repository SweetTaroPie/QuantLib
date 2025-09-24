/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 Neil Firth
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2003, 2004, 2005 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/pricingengines/barrier/mckikooptionengine.hpp>
#include <utility>

namespace QuantLib {

    KikoOptionPathPricer::KikoOptionPathPricer(const ext::shared_ptr<KIKOBarrier> kikoBarrier,
                                               Option::Type type,
                                               Real strike,
                                               std::vector<DiscountFactor> discounts,
                                               ext::shared_ptr<StochasticProcess1D> diffProcess,
                                               Size mBegin,
                                               Size mEnd)
    : kikoBarrier_(kikoBarrier), diffProcess_(std::move(diffProcess)), monitoringBegin_(mBegin),
      monitoringEnd_(mEnd), payoff_(type, strike), discounts_(std::move(discounts)) {

        QL_REQUIRE(strike >= 0.0, "strike less than zero not allowed");

        QL_REQUIRE(mBegin <= mEnd, "barrier start time greater than barrier end time");
    }

    Real KikoOptionPathPricer::KItime(const ext::shared_ptr<BarrierBase>& KiBarrier,
                                      const Path& path) const {
        Real time = QL_MAX_REAL;

        Real if_hit = false;

        for (Size i = monitoringBegin_; i < monitoringEnd_; ++i) {
            Time t = path.time(i);

            Real spot = path.at(i);

            // record the first hitting time for KI barrier
            if (!if_hit && KiBarrier->activated(spot, spot)) {
                time = t;

                if_hit = true;
            }
        }

        return time;
    }

    Real KikoOptionPathPricer::KOtime(const ext::shared_ptr<BarrierBase>& KoBarrier,
                                      const Path& path,
                                      KIKOBarrierType type) const {
        Real time = QL_MAX_REAL;

        // Real if_first_time = (type != KIKOBarrierType::After) ? true : false;

        Real if_hit = false;

        for (Size i = monitoringBegin_; i < monitoringEnd_; ++i) {
            Time t = path.time(i);

            Real spot = path.at(i);

            if (!if_hit && !KoBarrier->activated(spot, spot)) {

                time = t;

                if_hit = true;
            }

            if (!KoBarrier->activated(spot, spot) && type == KIKOBarrierType::After) {
                time = t;
            }
        }

        // if KIKOType == After, return the last hitting time
        // else return the first hitting time

        return time;
    }

    Real KikoOptionPathPricer::operator()(const Path& path) const {
        Size null = Null<Size>();
        Size n = path.length();
        QL_REQUIRE(n > 1, "the path cannot be empty");

        bool isOptionActive = false;
        Size knockNode = null;
        Real asset_price = path.back();
        Size i = 0;

        // Real runningMin = *std::min_element(path.begin() + monitoringBegin_, path.begin() +
        // monitoringEnd_); Real runningMax = *std::max_element(path.begin() + monitoringBegin_,
        // path.begin() + monitoringEnd_);

        Time KnockInTime = KItime(kikoBarrier_->KnockInBarrier(), path);

        Time KnockOutTime = KOtime(kikoBarrier_->KnockOutBarrier(), path, kikoBarrier_->kikoType());

        isOptionActive = kikoBarrier_->activated(KnockInTime, KnockOutTime);

        if (isOptionActive) {
            return payoff_(asset_price) * discounts_.back();
        } else {
            return kikoBarrier_->rebate() * discounts_.back();
        }

        // return payoff_(asset_price) * discounts_.back();
    }
}