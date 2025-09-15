#include "rwlocker.h"

// Αρχικοποιεί το αντικείμενο RWLocker:
// 1) Κατασκευάζει τους mutexes για τους αναγνώστες/γράφοντες.
// 2) Κατασκευάζει το condition variable για την επικοινωνία.
void rwlock_init(RWLocker *lock) {
    pthread_mutex_init(&lock->readers_mutex, NULL);       // Αρχικοποίηση mutex για τον μετρητή αναγνωστών
    pthread_mutex_init(&lock->writers_mutex, NULL);       // Αρχικοποίηση mutex για τους εγγραφείς
    pthread_cond_init(&lock->readers_finished_cond, NULL);// Αρχικοποίηση condition variable
    lock->readers_count = 0;                              // Μηδέν ενεργοί αναγνώστες
    lock->writer_active = 0;                              // Κανένας ενεργός εγγραφέας
}

// Καταστρέφει τους πόρους (mutexes και cond) όταν δεν χρειάζονται πλέον.
void rwlock_destroy(RWLocker *lock) {
    pthread_mutex_destroy(&lock->readers_mutex);          // Καταστροφή mutex αναγνωστών
    pthread_mutex_destroy(&lock->writers_mutex);          // Καταστροφή mutex εγγραφέων
    pthread_cond_destroy(&lock->readers_finished_cond);   // Καταστροφή condition variable
}

// Λειτουργία όταν κάποιος αναγνώστης (reader) θέλει να αποκτήσει πρόσβαση:
// 1) Κλειδώνουμε το readers_mutex για να προστατεύσουμε το readers_count.
// 2) Αν υπάρχει ενεργός writer (lock->writer_active == 1), περιμένουμε στο condition (readers_finished_cond).
// 3) Όταν δεν υπάρχει πλέον writer, αυξάνουμε το readers_count κατά 1.
// 4) Ξεκλειδώνουμε το readers_mutex.
void rwlock_reader_lock(RWLocker *lock) {
    pthread_mutex_lock(&lock->readers_mutex); // Προστασία μεταβλητής readers_count
    while (lock->writer_active) {             // Αν υπάρχει εγγραφέας, περιμένει
        pthread_cond_wait(&lock->readers_finished_cond, &lock->readers_mutex);
    }
    lock->readers_count++;                    // Αυξάνει τον αριθμό αναγνωστών
    pthread_mutex_unlock(&lock->readers_mutex); // Αποδέσμευση mutex
}

// Απελευθερώνει την πρόσβαση του αναγνώστη:
// 1) Κλειδώνουμε ξανά το readers_mutex για να μειώσουμε το readers_count.
// 2) Αν πέσει στο μηδέν, ειδοποιούμε με signal έναν πιθανό writer που περιμένει.
// 3) Ξεκλειδώνουμε.
void rwlock_reader_unlock(RWLocker *lock) {
    pthread_mutex_lock(&lock->readers_mutex); 
    lock->readers_count--;                   // Μειώνει τους ενεργούς αναγνώστες
    if (lock->readers_count == 0) {          // Αν δεν υπάρχουν άλλοι αναγνώστες
        pthread_cond_signal(&lock->readers_finished_cond); // Ειδοποιεί έναν εγγραφέα
    }
    pthread_mutex_unlock(&lock->readers_mutex);
}

// Απελευθερώνει την πρόσβαση του αναγνώστη:
// 1) Κλειδώνουμε ξανά το readers_mutex για να μειώσουμε το readers_count.
// 2) Αν πέσει στο μηδέν, ειδοποιούμε με signal έναν πιθανό writer που περιμένει.
// 3) Ξεκλειδώνουμε.
void rwlock_writer_lock(RWLocker *lock) {
    pthread_mutex_lock(&lock->writers_mutex);    // Αποκλειστικό mutex για εγγραφέα
    pthread_mutex_lock(&lock->readers_mutex);    // Προστατεύουμε readers_count
    lock->writer_active = 1;                     // Δηλώνουμε ενεργό εγγραφέα
    while (lock->readers_count > 0) {            // Αναμονή μέχρι να τελειώσουν οι αναγνώστες
        pthread_cond_wait(&lock->readers_finished_cond, &lock->readers_mutex);
    }
    pthread_mutex_unlock(&lock->readers_mutex);  // Αποδέσμευση readers mutex
}

// Όταν ο γράφων τελειώσει:
// 1) Παίρνει readers_mutex για να μη συμπέσει με νέους αναγνώστες.
// 2) Θέτει writer_active = 0, κάνει broadcast για να ξυπνήσει αναγνώστες ή άλλους γράφοντες.
// 3) Ξεκλειδώνει readers_mutex και writers_mutex.
void rwlock_writer_unlock(RWLocker *lock) {
    pthread_mutex_lock(&lock->readers_mutex); // Αποκλειστικό mutex για αναγνώστες
    lock->writer_active = 0; // Δηλώνουμε ανενεργό εγγραφέα
    pthread_cond_broadcast(&lock->readers_finished_cond); // Εκκίνηση όλων των αναγνωστών broadcast
    pthread_mutex_unlock(&lock->readers_mutex); // Αποδέσμευση readers mutex
    pthread_mutex_unlock(&lock->writers_mutex); // Αποδέσμευση writers mutex
}