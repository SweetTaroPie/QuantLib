#include "kikobarrier.h"

namespace QuantLib {
    KIKOBarrier::KIKOBarrier(const ext::shared_ptr<BarrierBase>& KIBarrier,
                             const KIKOBarrierType& type)
    : KIBarrier_(KIBarrier), kikoType_(type) {}

    SingleOutKikoBarrier::SingleOutKikoBarrier(const ext::shared_ptr<BarrierBase>& KIBarrier,
                                               const ext::shared_ptr<BarrierBase>& KOBarrier,
                                               const KIKOBarrierType& type)
    : KIKOBarrier(KIBarrier, type), KOBarrier_(KOBarrier) {}

    ext::shared_ptr<BarrierBase> SingleOutKikoBarrier::KnockOutBarrier(Size index) const {
        return KOBarrier_;
    }

    bool SingleOutKikoBarrier::activated(const Time& KnockInTime, const Time& KnockOutTime) const {
        // if knock-in barrier is never touched
        if (KnockInTime == QL_MAX_REAL) {
            return false;
        }

        switch (kikoType_) {
            case QuantLib::KIKOBarrierType::Any:
                // if knock-out barrier is never hit and knock-in barrier is hit, then activate the
                // option
                return KnockOutTime == QL_MAX_REAL && KnockInTime != QL_MAX_REAL;
                break;
            case QuantLib::KIKOBarrierType::Until:
                // knock-out barrier is effective until knock-in barrier is hit
                return (KnockOutTime > KnockInTime) || (KnockOutTime == QL_MAX_REAL);
                break;
            case QuantLib::KIKOBarrierType::After:
                // knock-out barrier is effective after knock-in barrier is hit
                return (KnockInTime > KnockOutTime) || (KnockOutTime == QL_MAX_REAL);
                break;
            default:
                break;
        }
    }

    DoubleOutKikoBarrier::DoubleOutKikoBarrier(const ext::shared_ptr<BarrierBase>& DoubleKIBarrier,
                                               const ext::shared_ptr<BarrierBase>& LowerKOBarrier,
                                               const ext::shared_ptr<BarrierBase>& UpperKOBarrier,
                                               const KIKOBarrierType& type)
    : KIKOBarrier(DoubleKIBarrier, type), LowerKOBarrier_(LowerKOBarrier),
      UpperKOBarrier_(UpperKOBarrier) {
        QL_REQUIRE(type == KIKOBarrierType::After, "Only KO After KI is supported.");
    }

    ext::shared_ptr<BarrierBase> DoubleOutKikoBarrier::LowerKnockOutBarrier(Size index) const {
        return LowerKOBarrier_;
    }

    ext::shared_ptr<BarrierBase> DoubleOutKikoBarrier::UpperKnockOutBarrier(Size index) const {
        return UpperKOBarrier_;
    }

    bool DoubleOutKikoBarrier::activated(const Time& KnockInTime, const Time& KnockOutTime) const {
        // too lazy to implement
        return true;
    }
}