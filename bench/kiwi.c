#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../engine/db.h"
#include "../engine/variant.h"
#include "bench.h"

#define DATAS ("testdb")

/* Global pointer για τη βάση – θα ανοίξει πριν ξεκινήσει το benchmark και θα κλείσει μετά. */
DB* globalDB = NULL;


// Σταματά τον timer και υπολογίζει τη διάρκεια
void calculateDuration(TimerData* timer) {
    clock_gettime(CLOCK_MONOTONIC, &timer->endTime);
    timer->totalTime = (timer->endTime.tv_sec - timer->startTime.tv_sec) +
                       (timer->endTime.tv_nsec - timer->startTime.tv_nsec) / 1E9;
}


/* -----------------------------------------------------------------------
   _write_test
   - Χρησιμοποιεί τον globalDB που έχει ήδη ανοιχτεί στην runBenchmark.
 */
 void* _write_test(void* arg)
{
	int i;
	//double cost;
	//long long start,end;
	Variant sk, sv;
	
	char key[KSIZE + 1];
	char val[VSIZE + 1];
	char sbuf[1024];

	memset(key, 0, KSIZE + 1);
	memset(val, 0, VSIZE + 1);
	memset(sbuf, 0, 1024);
	
	Datas* datas = (Datas*) arg;
    int threadIndex = datas->currentThreadIndex;  //  χρειάζεται για το συγκεκριμένο thread
    ThreadBenchArgs *tArgs = &datas->resultsPerOperation->threadArgs[threadIndex];
	
	// Εκκίνηση χρόνου για ολόκληρη την διαδικασία write αν ειναι το πρώτο νήμα
    if (tArgs->flagToStartStopTimeForThreadingOperation == 1)
        clock_gettime(CLOCK_MONOTONIC, &datas->resultsPerOperation->OperationTimer.startTime);
	
	// Ξεκινάμε και το δικό του timer του thread
    clock_gettime(CLOCK_MONOTONIC, &tArgs->timer.startTime);
    
	for (i = 0; i < tArgs->operationsPerThread; i++) {
		if (tArgs->isRandom)
			_random_key(key, KSIZE);
		else
			snprintf(key, KSIZE, "key-%d", i);
		fprintf(stderr, "%d adding %s\n", i, key);
		snprintf(val, VSIZE, "val-%d", i);
		
		sk.length = KSIZE;
		sk.mem = key;
		sv.length = VSIZE;
		sv.mem = val;
		
		db_add(globalDB, &sk, &sv);
		if ((i % 10000) == 0) {
			fprintf(stderr,"random write finished %d ops%30s\r", 
					i, 
					"");

			fflush(stderr);
		}
	}
	
	// Σταματάμε και το δικό του timer του thread
	clock_gettime(CLOCK_MONOTONIC, &tArgs->timer.endTime);
    calculateDuration(&tArgs->timer);

    // Λήξη timer του operation αν είναι το τελευταίο thread ή μοναδικό thread
	if (tArgs->flagToStartStopTimeForThreadingOperation == -1 || datas->resultsPerOperation->totalThreads == 1) {
		clock_gettime(CLOCK_MONOTONIC, &datas->resultsPerOperation->OperationTimer.endTime);
		calculateDuration(&datas->resultsPerOperation->OperationTimer);
	}
	pthread_exit(NULL);

}

void* _read_test(void* arg)
{
	int i;
	int ret;
	int found = 0;
	//double cost;
	//long long start,end;
	Variant sk;
	Variant sv;
	char key[KSIZE + 1];
	
	Datas* datas = (Datas*) arg;
    int threadIndex = datas->currentThreadIndex;  // για να γνωρίζουμε ποιο thread εκτελείται
    ThreadBenchArgs *tArgs = &datas->resultsPerOperation->threadArgs[threadIndex];
	
	// Εκκίνηση χρόνου για ολόκληρη την διαδικασία read αν ειναι το πρώτο νήμα
    if (tArgs->flagToStartStopTimeForThreadingOperation == 1)
        clock_gettime(CLOCK_MONOTONIC, &datas->resultsPerOperation->OperationTimer.startTime);

    // Ξεκινάμε και το δικό του timer του thread
    clock_gettime(CLOCK_MONOTONIC, &tArgs->timer.startTime);
    
	for (i = 0; i < tArgs->operationsPerThread; i++) {
		memset(key, 0, KSIZE + 1);

		/* if you want to test random write, use the following */
		if (tArgs->isRandom)
			_random_key(key, KSIZE);
		else
			snprintf(key, KSIZE, "key-%d", i);
		fprintf(stderr, "%d searching %s\n", i, key);
		
		sk.length = KSIZE;
		sk.mem = key;
		ret = db_get(globalDB, &sk, &sv);
		if (ret) {
			//db_free_data(sv.mem);
			found++;
		} else {
			INFO("not found key#%s", 
					sk.mem);
    	}

		if ((i % 10000) == 0) {
			fprintf(stderr,"random read finished %d ops%30s\r", 
					i, 
					"");

			fflush(stderr);
		}
	}
	
	// Κλειδιά που βρέθηκαν
	tArgs->keysFound = found;
	
	// Σταματάμε και το δικό του timer του thread
    clock_gettime(CLOCK_MONOTONIC, &tArgs->timer.endTime);
    calculateDuration(&tArgs->timer);

    // Λήξη timer του operation αν είναι το τελευταίο thread ή μοναδικό thread
	if (tArgs->flagToStartStopTimeForThreadingOperation == -1 || datas->resultsPerOperation->totalThreads == 1) {
		clock_gettime(CLOCK_MONOTONIC, &datas->resultsPerOperation->OperationTimer.endTime);
		calculateDuration(&datas->resultsPerOperation->OperationTimer);
	}
	pthread_exit(NULL);
}


/* Συνάρτηση partitionWorkload:
   Σκοπός της είναι να κατανείμει δίκαια τις εργασίες (operations) μεταξύ των threads,
   ανάλογα με τα ορίσματα που δίνει ο χρήστης (ανάγνωση/read, εγγραφή/write ή συνδυασμός/readwrite).

   Επιστρέφει ένα pointer τύπου Datas, ο οποίος περιέχει:
   - πληροφορίες για τις εργασίες που θα εκτελέσει κάθε thread μεσα στα benchresults τους,
   - κατανομή του αριθμού εργασιών σε κάθε thread με αρχικοποιημένους timers και flags.
*/

Datas* partitionWorkload(BasicBenchArgs* basic, AdvancedBenchArgs* adv) {
    // Δέσμευση μνήμης για τη βασική δομή δεδομένων της προσομοίωσης
    Datas *datas = calloc(1, sizeof(Datas));

    // Αποθηκεύουμε τα advanced args για μελλοντική χρήση στη δομή
    datas->advArgs = adv;

    // Καθορίζουμε αν η λειτουργία περιλαμβάνει αναγνώσεις (hasRead) και/ή εγγραφές (hasWrite)
    int hasRead = strcmp(basic->operationMode, "write") != 0;
    int hasWrite = strcmp(basic->operationMode, "read") != 0;

    // Υπολογισμός του αριθμού των τύπων εργασιών (1 ή 2)
    int opsCount = hasRead + hasWrite; 

    // Δεσμεύουμε μνήμη για την αποθήκευση αποτελεσμάτων ανά τύπο εργασίας (read και/ή write)
    datas->resultsPerOperation = calloc(opsCount, sizeof(BenchResults));

    // Συνολικός αριθμός threads από τα ορίσματα εισόδου
    int totalThreads = adv->threadCount;

    // Υπολογισμός αρχικού αριθμού threads ανά τύπο (με στρογγυλοποίηση προς τα πάνω για δικαιοσύνη)
    int readThreads = hasRead ? (int)(totalThreads * adv->readPercentage / 100.0 + 0.5) : 0;
    int writeThreads = totalThreads - readThreads;

    // Έλεγχος για αποφυγή περίπτωσης μηδενικού αριθμού threads σε κάποιο τύπο operation
    if (hasRead && readThreads == 0) { readThreads = 1; writeThreads--; }
    if (hasWrite && writeThreads == 0) { writeThreads = 1; readThreads--; }

    // Αρχικοποίηση δείκτη για να προσπελάσουμε τον πίνακα αποτελεσμάτων ανά operation
    int opIndex = 0;

    // Διαχείριση για την περίπτωση READ
    if (hasRead) {
        // Παίρνουμε δείκτη προς τα αποτελέσματα του READ operation
        BenchResults *readRes = &datas->resultsPerOperation[opIndex++];

        // Ορίζουμε αριθμό threads για το READ
        readRes->totalThreads = readThreads;

        // Συνολικός αριθμός εργασιών READ
        readRes->totalOperations = basic->operationCount * adv->readPercentage / 100;

        // Δέσμευση μνήμης για τα ορίσματα των read threads
        readRes->threadArgs = calloc(readThreads, sizeof(ThreadBenchArgs));

        // Υπολογισμός του αριθμού εργασιών ανά thread και του υπολοίπου
        long opsPerThread = readRes->totalOperations / readThreads;
        long extraOps = readRes->totalOperations % readThreads;

        // Διανομή εργασιών στα threads (δικαιοσύνη με έξτρα εργασίες στα πρώτα threads)
        for (int i=0; i<readThreads; i++) {
            readRes->threadArgs[i].operationsPerThread = opsPerThread + (i < extraOps ? 1 : 0);
            readRes->threadArgs[i].isReader = 1; // είναι thread ανάγνωσης
            readRes->threadArgs[i].isRandom = adv->isRandom;
            // Ορίζουμε ποιο thread ξεκινά και ποιο σταματά τον χρονομετρητή
            readRes->threadArgs[i].flagToStartStopTimeForThreadingOperation = 
                (i==0) ? 1 : (i==readThreads-1)? -1 : 0;
        }
    }

    // Διαχείριση για την περίπτωση WRITE
    if (hasWrite) {
        // Παίρνουμε δείκτη προς τα αποτελέσματα του WRITE operation
        BenchResults *writeRes = &datas->resultsPerOperation[opIndex];

        // Ορίζουμε αριθμό threads για το WRITE
        writeRes->totalThreads = writeThreads;

        // Συνολικός αριθμός εργασιών WRITE (όσες απέμειναν)
        writeRes->totalOperations = basic->operationCount - 
            (hasRead ? basic->operationCount * adv->readPercentage / 100 : 0);

        // Δέσμευση μνήμης για τα ορίσματα των write threads
        writeRes->threadArgs = calloc(writeThreads, sizeof(ThreadBenchArgs));

        // Υπολογισμός του αριθμού εργασιών ανά thread και του υπολοίπου
        long opsPerThread = writeRes->totalOperations / writeThreads;
        long extraOps = writeRes->totalOperations % writeThreads;

        // Διανομή εργασιών στα threads (δικαιοσύνη με έξτρα εργασίες στα πρώτα threads)
        for (int i=0; i<writeThreads; i++) {
            writeRes->threadArgs[i].operationsPerThread = opsPerThread + (i < extraOps ? 1 : 0);
            writeRes->threadArgs[i].isReader = 0; // είναι thread εγγραφής
            writeRes->threadArgs[i].isRandom = adv->isRandom;
            // Ορίζουμε ποιο thread ξεκινά και ποιο σταματά τον χρονομετρητή
            writeRes->threadArgs[i].flagToStartStopTimeForThreadingOperation = 
                (i==0) ? 1 : (i==writeThreads-1)? -1 : 0;
        }
    }

    // Επιστρέφουμε τη δομή με όλα τα δεδομένα της προσομοίωσης
    return datas;
}


/* computeResults:
   Υπολογίζει τα στατιστικά του benchmark:
   - Global totalCost από το totalTimer του BenchResults.
   - Συνολικό πλήθος keysRetrieved από τα read threads.
   - Μέσους χρόνους (readCost, writeCost) ως μέσος όρος των αντίστοιχων threads.
   - Ops/sec, sec/op, sec/thread.
*/
/// Συνάρτηση που υπολογίζει τα αποτελέσματα κάθε BenchResults
void computeResults(BenchResults *res) {
    long double totalTime = res->OperationTimer.totalTime;

    // Άθροισμα κλειδιών και χρόνων threads
    long keysRetrieved = 0;
    long double sumThreadsTime = 0;

    for (int i = 0; i < res->totalThreads; i++) {
        sumThreadsTime += res->threadArgs[i].timer.totalTime;

        // Μόνο αν είναι read προσθέτω keysFound
        if (res->threadArgs[i].isReader)
            keysRetrieved += res->threadArgs[i].keysFound;
    }

    // αποθηκεύω τα αποτελέσματα
    res->keysRetrieved = keysRetrieved;
    res->opsPerSecond = res->totalOperations / totalTime;
    res->secPerOp = totalTime / res->totalOperations;
    res->secPerThread = sumThreadsTime / res->totalThreads;
}

// Συνάρτηση που εμφανίζει τα αποτελέσματα στην οθόνη με συνολικό χρόνο benchmark
void printResults(BenchResults *res, BasicBenchArgs *basicArgs, AdvancedBenchArgs *advArgs, Datas *datas, int opIdx) {
	
	if(opIdx == 0){
		printf("========================================");
		printf("\n===== Ορίσματα benchmark =====\n");
		printf("Λειτουργία: %s\n", basicArgs->operationMode);
		printf("Συνολικές εργασίες: %ld\n", basicArgs->operationCount);
		printf("Αριθμός νημάτων: %ld\n", advArgs->threadCount);
		printf("Τυχαία πρόσβαση: %s\n", advArgs->isRandom ? "Ναι" : "Όχι");
		if (strcmp(basicArgs->operationMode, "readwrite") == 0) {
			printf("Ποσοστό αναγνώσεων: %.2f%%\n", advArgs->readPercentage);
		}

		// Εκτύπωση συνολικού χρόνου benchmark (global timer)
		printf("Συνολικός χρόνος Benchmark: %.4Lf sec\n", datas->GlobalTimer.totalTime);
	}

	printf("\n===== Αποτελέσματα benchmark %s =====\n", res->threadArgs[0].isReader ? "READ" : "WRITE");
	printf("Threads που χρησιμοποιήθηκαν: %ld\n", res->totalThreads);
	printf("Συνολικός αριθμός εργασιών: %ld\n", res->totalOperations);
	printf("Συνολικός χρόνος operation: %.4Lf sec\n", res->OperationTimer.totalTime);
	printf("Συνολική απόδοση (ops/sec): %.2Lf\n", res->opsPerSecond);
	printf("Μέσος χρόνος ανά εργασία (sec/op): %.6Lf sec\n", res->secPerOp);
	printf("Μέσος χρόνος ανά thread (sec/thread): %.6Lf sec\n", res->secPerThread);

	if (res->threadArgs[0].isReader) {
		printf("Συνολικά κλειδιά που βρέθηκαν: %ld\n", res->keysRetrieved);
	}
	
	// Το βγάζουμε αν θέλουμε να καταγράψουμε τα αποτελέσματα κάθε πειράματος
	//saveResultsToFile(res, basicArgs, advArgs, datas, opIdx);

}


void runBenchmark(BasicBenchArgs* basic, AdvancedBenchArgs* adv) {
	
	// Προετοιμασία εργασιών και νημάτων 
    Datas *datas = partitionWorkload(basic, adv);
	
	// Άνοιγμα βάσης δεδομένων
    globalDB = db_open(DATAS);

    // Ξεκινάμε το global timer του benchmark
    clock_gettime(CLOCK_MONOTONIC, &datas->GlobalTimer.startTime);

    // Υπολογίζουμε συνολικά πόσα threads έχουμε (read + write)
    int totalThreads = 0;
    int opsCount = (strcmp(basic->operationMode, "readwrite") == 0) ? 2 : 1;

    for (int j = 0; j < opsCount; j++)
        totalThreads += datas->resultsPerOperation[j].totalThreads;

    // Δημιουργούμε τα threads
    pthread_t *threads = malloc(sizeof(pthread_t) * totalThreads);
    Datas *threadDatas = malloc(sizeof(Datas) * totalThreads);

    int threadCounter = 0; // Μετρητής για όλα τα threads συνολικά

    for (int opIdx = 0; opIdx < opsCount; opIdx++) {
        BenchResults* opResult = &datas->resultsPerOperation[opIdx];

        for (int i = 0; i < opResult->totalThreads; i++) {
            threadDatas[threadCounter] = *datas; // αντιγραφή κοινών δεδομένων (struct copy)
            threadDatas[threadCounter].currentThreadIndex = i; // το index του thread μέσα στην ομάδα του
            threadDatas[threadCounter].resultsPerOperation = opResult; // δείκτης στο τρέχον BenchResults

            if (opResult->threadArgs[i].isReader)
                pthread_create(&threads[threadCounter], NULL, _read_test, &threadDatas[threadCounter]);
            else
                pthread_create(&threads[threadCounter], NULL, _write_test, &threadDatas[threadCounter]);

            threadCounter++;
        }
    }

    // join threads
    for (int i = 0; i < totalThreads; i++)
        pthread_join(threads[i], NULL);

    // Σταματάμε τον global timer του benchmark
    clock_gettime(CLOCK_MONOTONIC, &datas->GlobalTimer.endTime);
    calculateDuration(&datas->GlobalTimer);
	
	// Κλείσιμο της βάσης
    db_close(globalDB);
	
    // Υπολογισμός και εκτύπωση αποτελεσμάτων
	for (int opIdx = 0; opIdx < opsCount; opIdx++) {
		computeResults(&datas->resultsPerOperation[opIdx]);
		printResults(&datas->resultsPerOperation[opIdx], basic, adv, datas, opIdx);	
	}

	// Ελευθέρωση των thread 
    free(threads);
    free(threadDatas);

    // Αποδέσμευση μνήμης των threadArgs
    for (int opIdx = 0; opIdx < opsCount; opIdx++)
        free(datas->resultsPerOperation[opIdx].threadArgs);
	
    free(datas->resultsPerOperation);
    free(datas);
}


// Συνάρτηση που αποθηκεύει τα αποτελέσματα από κάθε benchmark σε ένα αρχείο benchmark_results.txt 
void saveResultsToFile(BenchResults *res, BasicBenchArgs *basicArgs, AdvancedBenchArgs *advArgs, Datas *datas, int opIdx) {
    FILE* file = fopen("/home/myy601/kiwi/kiwi-source/bench/benchmark_results.txt", "a"); // append mode
	
    if(opIdx == 0){
		fprintf(file, "========================================");
        fprintf(file, "\n===== Ορίσματα benchmark =====\n");
        fprintf(file, "Λειτουργία: %s\n", basicArgs->operationMode);
        fprintf(file, "Συνολικές εργασίες: %ld\n", basicArgs->operationCount);
        fprintf(file, "Αριθμός νημάτων: %ld\n", advArgs->threadCount);
        fprintf(file, "Τυχαία πρόσβαση: %s\n", advArgs->isRandom ? "Ναι" : "Όχι");
        if (strcmp(basicArgs->operationMode, "readwrite") == 0) {
            fprintf(file, "Ποσοστό αναγνώσεων: %.2f%%\n", advArgs->readPercentage);
        }
        fprintf(file, "Συνολικός χρόνος Benchmark: %.4Lf sec\n", datas->GlobalTimer.totalTime);
    }

    fprintf(file, "\n===== Αποτελέσματα benchmark %s =====\n", res->threadArgs[0].isReader ? "READ" : "WRITE");
    fprintf(file, "Threads που χρησιμοποιήθηκαν: %ld\n", res->totalThreads);
    fprintf(file, "Συνολικός αριθμός εργασιών: %ld\n", res->totalOperations);
    fprintf(file, "Συνολικός χρόνος operation: %.4Lf sec\n", res->OperationTimer.totalTime);
    fprintf(file, "Συνολική απόδοση (ops/sec): %.2Lf\n", res->opsPerSecond);
    fprintf(file, "Μέσος χρόνος ανά εργασία (sec/op): %.6Lf sec\n", res->secPerOp);
    fprintf(file, "Μέσος χρόνος ανά thread (sec/thread): %.6Lf sec\n", res->secPerThread);

    if (res->threadArgs[0].isReader) {
        fprintf(file, "Συνολικά κλειδιά που βρέθηκαν: %ld\n", res->keysRetrieved);
    }

    fclose(file);
}