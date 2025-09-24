#ifndef quantlib_kiko_barriers_h
#define quantlib_kiko_barriers_h



#include <ql/instruments/barriers/barriers.h>
#include <ql/time/all.hpp>
#include <iostream>

namespace QuantLib {
    enum class KIKOBarrierType {
        Any,   // knock-in knock-out anytime
        Until, // knock-out before knock-in
        After  // knock-out after knock-out
    };

    class KIKOBarrier {
      public:
        KIKOBarrier(const ext::shared_ptr<BarrierBase>& KIBarrier, const KIKOBarrierType& type);

      public:
        virtual bool activated(const Time& KnockInTime, const Time& KnockOutTime) const = 0;

      public:
        KIKOBarrierType kikoType() const { return kikoType_; }

        Real Rebate() const { return rebate_; }

        ext::shared_ptr<BarrierBase> KnockInBarrier() const { return KIBarrier_; }

        virtual ext::shared_ptr<BarrierBase> KnockOutBarrier(Size index = 0) const = 0;
        virtual ext::shared_ptr<BarrierBase> LowerKnockOutBarrier(Size index = 0) const = 0;
        virtual ext::shared_ptr<BarrierBase> UpperKnockOutBarrier(Size index = 0) const = 0;

        MonitoringType monitoringType() const { return KIBarrier_->monitoringType(); }

        Date windowStart() const { return KIBarrier_->windowStart(); }

        Date windowEnd() const { return KIBarrier_->windowEnd(); }

        std::vector<Date> monitoringDates() const { return KIBarrier_->monitoringDates(); }

        Real rebate() const { return KnockOutBarrier()->rebate(); }

      protected:
        ext::shared_ptr<BarrierBase> KIBarrier_;
        KIKOBarrierType kikoType_;
        Real rebate_;
    };

    class SingleOutKikoBarrier : public KIKOBarrier {
        /* Kiko Barrier contains only one Knock-Out barrier */
      public:
        SingleOutKikoBarrier(const ext::shared_ptr<BarrierBase>& KIBarrier,
                             const ext::shared_ptr<BarrierBase>& KOBarrier,
                             const KIKOBarrierType& type);

      public:
        virtual bool activated(const Time& KnockInTime, const Time& KnockOutTime) const;

        virtual ext::shared_ptr<BarrierBase> KnockOutBarrier(Size index = 0) const;

        ext::shared_ptr<BarrierBase> LowerKnockOutBarrier(Size index = 0) const override {
            return ext::shared_ptr<BarrierBase>();
        }

        ext::shared_ptr<BarrierBase> UpperKnockOutBarrier(Size index = 0) const override {
            return ext::shared_ptr<BarrierBase>();
        }

      private:
        ext::shared_ptr<BarrierBase> KOBarrier_;
    };

    class DoubleOutKikoBarrier : public KIKOBarrier {
        /* Kiko Barrier contains two Knock-Out barriers,
           for After only, with Double-In type for KI
        */
      public:
        DoubleOutKikoBarrier(const ext::shared_ptr<BarrierBase>& DoubleKIBarrier,
                             const ext::shared_ptr<BarrierBase>& LowerKOBarrier,
                             const ext::shared_ptr<BarrierBase>& UpperKOBarrier,
                             const KIKOBarrierType& type = KIKOBarrierType::After);

      public:
        virtual bool activated(const Time& KnockInTime, const Time& KnockOutTime) const;

        ext::shared_ptr<BarrierBase> KnockOutBarrier(Size index = 0) const override {
            return ext::shared_ptr<BarrierBase>();
        }

        virtual ext::shared_ptr<BarrierBase> LowerKnockOutBarrier(Size index = 0) const;

        virtual ext::shared_ptr<BarrierBase> UpperKnockOutBarrier(Size index = 0) const;

      private:
        ext::shared_ptr<BarrierBase> LowerKOBarrier_;
        ext::shared_ptr<BarrierBase> UpperKOBarrier_;
    };
}

#endif