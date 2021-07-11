#ifndef __STDATOMIC_H
#define __STDATOMIC_H

#define atomic_compare_exchange_weak(obj, old_val, new_val) __builtin_compare_and_swap((obj), (old_val), (new_val))
#define atomic_compare_exchange_strong(obj, old_val, new_val) __builtin_compare_and_swap((obj), (old_val), (new_val))
#define atomic_exchange(obj, val) __builtin_atomic_exchange((obj), (val))
#define atomic_exchange_explicit(obj, val, order) __builtin_atomic_exchange((obj), (val))

#endif