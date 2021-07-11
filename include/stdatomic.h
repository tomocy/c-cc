#ifndef __STDATOMIC_H
#define __STDATOMIC_H

#define atomic_compare_exchange_weak(p, old_val, new_val) __builtin_compare_and_swap((p), (old_val), (new_val))
#define atomic_compare_exchange_strong(p, old_val, new_val) __builtin_compare_and_swap((p), (old_val), (new_val))

#endif