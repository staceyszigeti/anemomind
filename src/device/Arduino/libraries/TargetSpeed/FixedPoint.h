// (c) 2014 Julien Pilet <julien.pilet@gmail.com>

#ifndef DEVICE_FIXED_POINT_H
#define DEVICE_FIXED_POINT_H

#include <stdint.h>

template <typename StoreType, typename LongType, int Shift>
class FixedPoint {
 public:
   // typedefs
   typedef FixedPoint<StoreType, LongType, Shift> ThisType;

   // Constructors
   FixedPoint() { }
   FixedPoint(int x) { _value = x << Shift; }
   FixedPoint(float x) { _value = StoreType(x * (1 << Shift)); }
   FixedPoint(double x) { _value = StoreType(x * (1 << Shift)); }
   FixedPoint(const FixedPoint &a) : _value(a._value) { }

   // Named constructors
   static ThisType rightShiftAndConstruct(LongType x, int shift) {
     LongType val = x;
     if (shift > Shift) {
       val >>= (shift - Shift);
     }
     if (Shift > shift) {
       val <<= (Shift - shift);
     }
     return make(val);
   }

   // Convert any FixedPoint to our type.
   template <typename FromStoreType, typename FromLongType, int FromShift>
   static ThisType convert(FixedPoint<FromStoreType, FromLongType, FromShift> a) {
     return rightShiftAndConstruct(a._value, FromShift);
   }

   // Casts
   operator int() const { return int(LongType(_value) >> Shift); }
   operator float() const { return float(_value) / float(1 << Shift); }
   operator double() const { return double(_value) / double(1 << Shift); }

   // const operators
   ThisType operator + (ThisType other) const {
     return make(_value + other._value);
   }
   ThisType operator - (ThisType other) const {
     return make(_value - other._value);
   }
   ThisType operator * (ThisType other) const {
     return make(StoreType((LongType(_value) * LongType(other._value)) >> Shift));
   }
   ThisType operator / (ThisType other) const {
     return make(StoreType((LongType(_value) << Shift) / LongType(other._value)));
   }
   bool operator < (ThisType other) const { return _value < other._value; }
   bool operator <= (ThisType other) const { return _value <= other._value; }
   bool operator == (ThisType other) const { return _value == other._value; }
   bool operator > (ThisType other) const { return _value > other._value; }
   bool operator >= (ThisType other) const { return _value >= other._value; }

   // in-place operators
   ThisType &operator += (ThisType other) { _value += other._value; return *this; }
   ThisType &operator -= (ThisType other) { _value -= other._value; return *this; }
   ThisType &operator *= (ThisType other) {
     _value = StoreType((LongType(_value) * LongType(other._value)) >> Shift);
     return *this; 
   }
   ThisType &operator /= (ThisType other) {
     _value = StoreType((LongType(_value) << Shift) / LongType(other._value));
     return *this; 
   }

 private:
   static ThisType make(StoreType value) {
     ThisType result;
     result._value = value;
     return result;
   }

   StoreType _value;
};

typedef FixedPoint<int16_t, int32_t, 8> FP8_8;
typedef FixedPoint<int32_t, int64_t, 16> FP16_16;

#endif // DEVICE_FIXED_POINT_H
