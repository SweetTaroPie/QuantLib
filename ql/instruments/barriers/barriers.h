#ifndef quantlib_customized_barriers_h
#define quantlib_customized_barriers_h

/*
 * self-developed barrier types by:
 *   Yue Zhang
 */

#include <ql/time/all.hpp>
#include <iostream>

namespace QuantLib {
    enum class SimpleBarrierType { UI, UO, DI, DO };

    enum class DoubleBarrierType { In, Out };

    enum class MonitoringType { Continuous, Discrete };

    class BarrierBase {
      public:
        BarrierBase(const Date& windowStart,
                    const Date& windowEnd,
                    const MonitoringType& monitoring,
                    const Real& rebate = Null<Real>(),
                    const Frequency& freq = Null<Frequency>());

      public:
        Date windowStart() const { return windowStart_; }

        Date windowEnd() const { return windowEnd_; }

        double rebate() const { return rebate_; }

        std::vector<Date> monitoringDates() const {
            if (monitoring_ == MonitoringType::Discrete) {
                if (windowStart_ < windowEnd_) {
                    Schedule sch = MakeSchedule()
                                       .from(windowStart_)
                                       .to(windowEnd_)
                                       .withFrequency(freq_)
                                       .forwards();

                    return sch.dates();
                } else {
                    return std::vector<Date>({windowStart_});
                }
            } else {
                throw("Barrier.monitoringDates(): only discrete type can produce monitoring dates");
            }
        }

        MonitoringType monitoringType() const { return monitoring_; }

        virtual bool activated(const double& runningMax, const double& runningMin) const = 0;

      private:
        Date windowStart_, windowEnd_;
        double rebate_;
        MonitoringType monitoring_;
        Frequency freq_;
    };

    class SimpleBarrier : public BarrierBase {
      public:
        SimpleBarrier(const Date& windowStart,
                      const Date& windowEnd,
                      const double& barrier,
                      const MonitoringType& monitoring,
                      const SimpleBarrierType& type,
                      const double& rebate = Null<Real>(),
                      const Frequency& freq = Null<Frequency>());

      public:
        double barrier() const { return barrier_; }

        virtual bool activated(const double& runningMax, const double& runningMin) const;

        SimpleBarrierType type() const { return type_; }

      private:
        double barrier_;
        SimpleBarrierType type_;
    };

    class DoubleBarrier : public BarrierBase {
      public:
        DoubleBarrier(const Date& windowStart,
                      const Date& windowEnd,
                      const double& upperBarrier,
                      const double& lowerBarrier,
                      const MonitoringType& monitoring,
                      const DoubleBarrierType& type,
                      const double& rebate = Null<Real>(),
                      const Frequency& freq = Null<Frequency>());

      public:
        double upperBarrier() const { return upper_; }

        double lowerBarrier() const { return lower_; }

        DoubleBarrierType type() const { return type_; }

      public:
        virtual bool activated(const double& runningMax, const double& runningMin) const;

      private:
        double upper_, lower_;
        DoubleBarrierType type_;
    };
}

#endif // !QALIB_BARRIER_H