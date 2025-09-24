#include <ql/instruments/barriers/barriers.h>

namespace QuantLib {
    BarrierBase::BarrierBase(const Date& windowStart,
                             const Date& windowEnd,
                             const MonitoringType& monitoring,
                             const Real& rebate,
                             const Frequency& freq)
    : windowStart_(windowStart), windowEnd_(windowEnd), monitoring_(monitoring), rebate_(rebate),
      freq_(freq) {}

    SimpleBarrier::SimpleBarrier(const Date& windowStart,
                                 const Date& windowEnd,
                                 const double& barrier,
                                 const MonitoringType& monitoring,
                                 const SimpleBarrierType& type,
                                 const double& rebate,
                                 const Frequency& freq)
    : BarrierBase(windowStart, windowEnd, monitoring, rebate, freq), type_(type),
      barrier_(barrier) {}

    bool SimpleBarrier::activated(const double& runningMax, const double& runningMin) const {
        if (type_ == SimpleBarrierType::UI) {
            // up in
            return (runningMax >= barrier_);
        } else if (type_ == SimpleBarrierType::UO) {
            return (runningMax < barrier_);
        } else if (type_ == SimpleBarrierType::DI) {
            return (runningMin <= barrier_);
        } else {
            // down out
            return (runningMin > barrier_);
        }
    }

    DoubleBarrier::DoubleBarrier(const Date& windowStart,
                                 const Date& windowEnd,
                                 const double& upperBarrier,
                                 const double& lowerBarrier,
                                 const MonitoringType& monitoring,
                                 const DoubleBarrierType& type,
                                 const double& rebate,
                                 const Frequency& freq)
    : BarrierBase(windowStart, windowEnd, monitoring, rebate, freq), upper_(upperBarrier),
      lower_(lowerBarrier), type_(type) {}

    bool DoubleBarrier::activated(const double& runningMax, const double& runningMin) const {
        if (type_ == DoubleBarrierType::In) {
            // in when one barrier is touched
            return (runningMax >= upper_) || (runningMin <= lower_);
        } else if (type_ == DoubleBarrierType::Out) {
            // out when one barrier is touched
            return (runningMax < upper_) && (runningMin > lower_);
        }
    }
}