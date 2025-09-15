#ifndef RWLOCK_PRIORITY_H
#define RWLOCK_PRIORITY_H

#include <pthread.h>
typedef struct {
    pthread_mutex_t readers_mutex;      // Mutex για προστασία του μετρητή readers_count
    pthread_mutex_t writers_mutex;      // Mutex για την αποκλειστική πρόσβαση από writer
    pthread_cond_t readers_finished_cond; // Condition variable για αναμονή ολοκλήρωσης των αναγνωστών
    int readers_count;                  // Αριθμός αναγνωστών που είναι ενεργοί
    int writer_active;                  // Ένδειξη αν υπάρχει writer ενεργός (0/1)
} RWLocker;

// prototypes 
void rwlock_init(RWLocker *lock);
void rwlock_destroy(RWLocker *lock);
void rwlock_reader_lock(RWLocker *lock);
void rwlock_reader_unlock(RWLocker *lock);
void rwlock_writer_lock(RWLocker *lock);
void rwlock_writer_unlock(RWLocker *lock);

#endif // RWLOCK_PRIORITY_H
