#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define KSIZE (16)
#define VSIZE (1000)

#define LINE "+-----------------------------+----------------+------------------------------+-------------------+\n"
#define LINE1 "---------------------------------------------------------------------------------------------------\n"

/* Συνάρτηση για λήψη του τρέχοντος χρόνου σε μικροδευτερόλεπτα (αν χρειαστεί) */
long long get_ustime_sec(void);

/* Συνάρτηση για δημιουργία τυχαίου κλειδιού με δεδομένο μήκος */
void _random_key(char *key,int length);

/* Δομή για τα βασικά ορίσματα του benchmark (υποχρεωτικά) */
typedef struct {
    char *operationMode;      // Λειτουργία: "read", "write" ή "readwrite"
    long int operationCount;  // Συνολικός αριθμός εργασιών
} BasicBenchArgs;

typedef struct {
    long int threadCount;       // Συνολικός αριθμός των threads
    double readPercentage;      // Ποσοστό read, π.χ. 50% (μόνο για readwrite mode)
    int isRandom;               // Τυχαία πρόσβαση (1) ή σειριακή (0)
} AdvancedBenchArgs;

/* Δομή για προεπιλεγμένες τιμές ορισμάτων */
typedef struct {
    long int defaultThreadCount;
    double defaultReadPercentage;
    int defaultRandom;
} DefaultArgs;

// Δομη για αποθήκευση χρόνου
typedef struct {
    struct timespec startTime;  // χρόνος έναρξης
    struct timespec endTime;    // χρόνος λήξης
    long double totalTime;      // Συνολικός χρόνος σε sec
} TimerData;

/* Δομή για τα ορίσματα κάθε νηματικής εργασίας */
typedef struct {
    TimerData timer;      // ατομικός χρόνος εκτέλεσης για το συγκεκριμένο thread
    int flagToStartStopTimeForThreadingOperation; 
        // = 1 για το πρώτο thread μιας ομάδας (read ή write)
        // = -1 για το τελευταίο thread μιας ομάδας (read ή write)
        // = 0 για όλα τα ενδιάμεσα threads
    long operationsPerThread;  // αριθμός εργασιών που θα κάνει το thread
    int isReader;              // αν είναι read-thread: 1, αλλιώς 0
	int isRandom;
    long keysFound;            // αριθμός κλειδιών που βρέθηκαν (μόνο για read)
} ThreadBenchArgs;

/* Δομή για αποθήκευση των αποτελεσμάτων του καθε operation */
typedef struct {
    ThreadBenchArgs* threadArgs; // πίνακας με structs των threads
    TimerData OperationTimer;    // TimerData για συνολικό χρόνο read ή write ομάδας
    long int keysRetrieved;      // κλειδιά που βρέθηκαν (μόνο για read threads)
    long int totalOperations;    // συνολικός αριθμός εργασιών (όλων των threads μαζί)
    long int totalThreads;       // συνολικά threads που χρησιμοποιήθηκαν
    long double opsPerSecond;    // συνολική απόδοση εργασιών/sec
    long double secPerOp;        // χρόνος ανά εργασία (sec)
    long double secPerThread;    // μέσος χρόνος ανά thread (sec)
} BenchResults;

// Φωλιασμα όλων των δεδομένων για τα test και τα αποτελέσματα
typedef struct {
    TimerData GlobalTimer;         // TimerData συνολικού benchmark (start-end)
    AdvancedBenchArgs *advArgs;		// 
    BenchResults *resultsPerOperation; // Array (read/write results)
	int currentThreadIndex; 
} Datas;

/* Ορισμός κωδικών σφαλμάτων */
typedef enum {
    ERR_NONE = 0,
    ERR_INSUFFICIENT_ARGS,
    ERR_INVALID_OPERATION,
    ERR_INVALID_COUNT,
    ERR_INVALID_THREADS,
    ERR_INVALID_READ_PERCENTAGE
} ErrorCode;

/* Δομή για πληροφορίες σφαλμάτων */
typedef struct {
    ErrorCode code;
    char message[256];
} ErrorHandler;



