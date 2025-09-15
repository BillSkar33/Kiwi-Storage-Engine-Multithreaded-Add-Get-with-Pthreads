#include "bench.h"

void _random_key(char *key,int length) {
	int i;
	char salt[36]= "abcdefghijklmnopqrstuvwxyz0123456789";

	for (i = 0; i < length; i++)
		key[i] = salt[rand() % 36];
}

void _print_header(int count)
{
	double index_size = (double)((double)(KSIZE + 8 + 1) * count) / 1048576.0;
	double data_size = (double)((double)(VSIZE + 4) * count) / 1048576.0;

	printf("Keys:\t\t%d bytes each\n", 
			KSIZE);
	printf("Values: \t%d bytes each\n", 
			VSIZE);
	printf("Entries:\t%d\n", 
			count);
	printf("IndexSize:\t%.1f MB (estimated)\n",
			index_size);
	printf("DataSize:\t%.1f MB (estimated)\n",
			data_size);

	printf(LINE1);
}

void _print_environment()
{
	time_t now = time(NULL);

	printf("Date:\t\t%s", 
			(char*)ctime(&now));

	int num_cpus = 0;
	char cpu_type[256] = {0};
	char cache_size[256] = {0};

	FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
	if (cpuinfo) {
		char line[1024] = {0};
		while (fgets(line, sizeof(line), cpuinfo) != NULL) {
			const char* sep = strchr(line, ':');
			if (sep == NULL || strlen(sep) < 10)
				continue;

			char key[1024] = {0};
			char val[1024] = {0};
			strncpy(key, line, sep-1-line);
			strncpy(val, sep+1, strlen(sep)-1);
			if (strcmp("model name", key) == 0) {
				num_cpus++;
				strcpy(cpu_type, val);
			}
			else if (strcmp("cache size", key) == 0)
				strncpy(cache_size, val + 1, strlen(val) - 1);	
		}

		fclose(cpuinfo);
		printf("CPU:\t\t%d * %s", 
				num_cpus, 
				cpu_type);

		printf("CPUCache:\t%s\n", 
				cache_size);
	}
}

/* Συνάρτηση για διαχείριση σφαλμάτων με χρήση switch-case */
void handleError(const ErrorHandler err) {
    fprintf(stderr, "Benchmark Error: ");
    switch(err.code) {
        case ERR_INSUFFICIENT_ARGS:
            fprintf(stderr, "Ανεπαρκή ορίσματα. Χρήση: ./kiwi-bench <read | write | readwrite> <count> [<threads> <random> [<read_percentage>]]\n");
            break;
        case ERR_INVALID_OPERATION:
            fprintf(stderr, "Μη έγκυρη λειτουργία. Επιλέξτε 'read', 'write' ή 'readwrite'.\n");
            break;
        case ERR_INVALID_COUNT:
            fprintf(stderr, "Ο αριθμός εργασιών πρέπει να είναι μεγαλύτερος του 0.\n");
            break;
        case ERR_INVALID_THREADS:
            fprintf(stderr, "Ο αριθμός νημάτων πρέπει να είναι μεγαλύτερος του 0. Για λειτουργία readwrite απαιτούνται τουλάχιστον 2 νήματα.\n");
            break;
        case ERR_INVALID_READ_PERCENTAGE:
            fprintf(stderr, "Το ποσοστό αναγνώσεων πρέπει να είναι > 0 και μικρότερο του 100.\n");
            break;
        default:
            fprintf(stderr, "Άγνωστο σφάλμα.\n");
            break;
    }
    exit(EXIT_FAILURE);
}

/* Συνάρτηση για ανάλυση των ορισμάτων (argc, argv) και ανάθεση στις δομές BasicBenchArgs και AdvancedBenchArgs */
void parseBenchArgs(int argc, char **argv, BasicBenchArgs *basic, AdvancedBenchArgs *advanced) {
    ErrorHandler err;
    /* Ορισμός προεπιλεγμένων τιμών */
    DefaultArgs defaults = {1, 50.0, 0};  // 1 νήμα, 50% read (για readwrite), random = 0

    /* Έλεγχος συνολικού αριθμού ορισμάτων (πρέπει να είναι από 3 έως 6) */
    if (argc < 3 || argc > 6) {
        err.code = ERR_INSUFFICIENT_ARGS;
        strcpy(err.message, "Λείπουν ή υπάρχουν επιπλέον ορίσματα.");
        handleError(err);
    }

    /* Ανάθεση βασικών ορισμάτων */
    basic->operationMode = argv[1];
    if (strcmp(basic->operationMode, "read") != 0 &&
        strcmp(basic->operationMode, "write") != 0 &&
        strcmp(basic->operationMode, "readwrite") != 0) {
        err.code = ERR_INVALID_OPERATION;
        strcpy(err.message, "Οι λειτουργίες είναι μόνο 'read', 'write' ή 'readwrite'.");
        handleError(err);
    }

    basic->operationCount = atol(argv[2]);
    if (basic->operationCount <= 0) {
        err.code = ERR_INVALID_COUNT;
        strcpy(err.message, "Ο αριθμός εργασιών πρέπει να είναι μεγαλύτερος του 0.");
        handleError(err);
    }

    /* Επεξεργασία προαιρετικών ορισμάτων ανάλογα με τη λειτουργία */
    if (strcmp(basic->operationMode, "readwrite") == 0) {
		if (argc < 4) {
			err.code = ERR_INSUFFICIENT_ARGS;
			strcpy(err.message, "Για λειτουργία readwrite απαιτείται τουλάχιστον <threads>.");
			handleError(err);
		}

		advanced->threadCount = atol(argv[3]);
		if (advanced->threadCount < 2) {
			err.code = ERR_INVALID_THREADS;
			strcpy(err.message, "Για λειτουργία readwrite απαιτούνται τουλάχιστον 2 νήματα.");
			handleError(err);
		}

		// Αν έχουμε τουλάχιστον 5 args, τότε το 5ο είναι το random
		if (argc >= 5) {
			advanced->isRandom = atoi(argv[4]);
		} else {
			advanced->isRandom = defaults.defaultRandom;
		}

		// Αν έχουμε 6 args, τότε το 6ο είναι το readPercentage
		if (argc == 6) {
			advanced->readPercentage = atof(argv[5]);
			if (advanced->readPercentage <= 0 || advanced->readPercentage >= 100) {
				err.code = ERR_INVALID_READ_PERCENTAGE;
				strcpy(err.message, "Το ποσοστό αναγνώσεων πρέπει να είναι > 0 και < 100.");
				handleError(err);
			}
		} else {
			advanced->readPercentage = defaults.defaultReadPercentage;
		}
	}

    else if (strcmp(basic->operationMode, "read") == 0) {
        /* Για read: αν υπάρχει 4ο όρισμα, τότε αυτό είναι ο αριθμός νημάτων, και αν υπάρχει 5ο, αυτό το isRandom */
        if (argc >= 4) {
            advanced->threadCount = atol(argv[3]);
        } else {
            advanced->threadCount = defaults.defaultThreadCount;
        }
        if (advanced->threadCount <= 0) {
            err.code = ERR_INVALID_THREADS;
            strcpy(err.message, "Ο αριθμός νημάτων πρέπει να είναι μεγαλύτερος του 0.");
            handleError(err);
        }
        if (argc >= 5) {
            advanced->isRandom = atoi(argv[4]);
        } else {
            advanced->isRandom = defaults.defaultRandom;
        }
        advanced->readPercentage = 100.0;  // Όλες οι εργασίες είναι αναγνώσεις.
    }
    else if (strcmp(basic->operationMode, "write") == 0) {
        /* Για write: αν υπάρχει 4ο όρισμα, τότε αυτός είναι ο αριθμός νημάτων, και αν υπάρχει 5ο, αυτό το isRandom */
        if (argc >= 4) {
            advanced->threadCount = atol(argv[3]);
        } else {
            advanced->threadCount = defaults.defaultThreadCount;
        }
        if (advanced->threadCount <= 0) {
            err.code = ERR_INVALID_THREADS;
            strcpy(err.message, "Ο αριθμός νημάτων πρέπει να είναι μεγαλύτερος του 0.");
            handleError(err);
        }
        if (argc >= 5) {
            advanced->isRandom = atoi(argv[4]);
        } else {
            advanced->isRandom = defaults.defaultRandom;
        }
        advanced->readPercentage = 0.0;  // Όλες οι εργασίες είναι εγγραφές.
    }
}


/* Κύρια συνάρτηση του benchmark */
int main(int argc, char **argv) {
    BasicBenchArgs basicArgs;
    AdvancedBenchArgs advArgs;
    
    /* Αρχικοποίηση τυχαίων αριθμών και εκτύπωση header και περιβάλλοντος */
    srand(time(NULL));
   /* Εκτύπωση header και περιβαλλοντικών πληροφοριών */
    _print_header((argc > 2) ? atoi(argv[2]) : 0);
    _print_environment();
	
    /* Ανάλυση ορισμάτων από τη γραμμή εντολών */
    parseBenchArgs(argc, argv, &basicArgs, &advArgs);
	
    // Εκκίνηση benchmark
    runBenchmark(&basicArgs, &advArgs);
    
    return EXIT_SUCCESS;
}
