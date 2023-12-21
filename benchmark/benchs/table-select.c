#include "../src/bench.h"
#include "backend/table/table.h"
#include "utils/logger.h"
#include <sys/time.h>
#include <inttypes.h>
#if __linux__
#include <stdio.h>
#include <time.h>

#endif

const char* TEST_DB = "test.db";
const char* CSV_FILE = "table-select.csv";
const char* CSV_HEADER= "Time;Allocated\n";
const int TEST_TIME = 2*60;
const int ALLOCATION = 50;
const int SELECT = 30;

struct timespec start, end;

void insert_rows(table_t* table, schema_t* schema, int64_t start_index, int64_t number_of_rows) {
    tab_row(
            int64_t ID;
            char NAME[10];
            float SCORE;
            int64_t AGE;
            bool PASS;
    );
    for (int64_t index = start_index; index < start_index + number_of_rows; ++index) {
        row.ID = index;
        row.SCORE = 9.9f;
        row.AGE = index;
        row.PASS = true;
        chblix_t block = tab_insert(table, schema, &row);
//            printf("id: %lld\n", j);
        if (chblix_cmp(&block, &CHBLIX_FAIL) == 0) {
            logger(LL_ERROR, __func__, "Failed to insert row ");
            return;
        }
    }
}

int select_rows(FILE* file, db_t* db, table_t* table, schema_t* schema, field_t* field, int64_t start_index, int64_t number_of_rows, int64_t rows_inserted) {
    tab_row(
            int64_t ID;
            char NAME[10];
            float SCORE;
            int64_t AGE;
            bool PASS;
    );
    for (int64_t index = start_index; index < start_index + number_of_rows; ++index) {
        clock_gettime(CLOCK_UPTIME_RAW, &start);
        table_t* sel_table = tab_select_op(db, table, schema, field, "SELECT", COND_EQ, &index, DT_INT);
        clock_gettime(CLOCK_UPTIME_RAW, &end);
        if (sel_table == NULL) {
            logger(LL_ERROR, __func__, "Failed to delete row ");
            return -1;
        }
        tab_drop(db,sel_table);
    }
    int64_t delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
    fprintf(file, "%f;%"PRId64"\n", (double)delta_us / (double)SELECT, rows_inserted);
    fflush(file);
    return 0;
}


int main(){
    db_t* db = db_init(TEST_DB);
    if(pg_file_size() > 0){
        db_drop();
        db = db_init(TEST_DB);
    }
    sleep(5);
    FILE* file = fopen(CSV_FILE, "w+");
    fprintf(file, "%s", CSV_HEADER);
    /* Create table */
    schema_t* schema = sch_init();
    sch_add_int_field(schema, "ID");
    sch_add_char_field(schema, "NAME", 10);
    sch_add_float_field(schema, "SCORE");
    sch_add_int_field(schema, "AGE");
    sch_add_bool_field(schema, "PASS");
    table_t* table = tab_init(db, "STUDENT", schema);
    tab_row(
            int64_t ID;
            char NAME[10];
            float SCORE;
            int64_t AGE;
            bool PASS;
    );
    field_t field;
    if(sch_get_field(schema, "ID", &field) == SCHEMA_FAIL){
        return TABLE_FAIL;
    }
    time_t test_start = time(NULL);
    time_t test_end = time(NULL);
    int64_t i = 0;
    int64_t rows_inserted = 0;
    int64_t next_insert_start = 0;
    while(test_end - test_start < TEST_TIME) {
        insert_rows(table, schema, next_insert_start, ALLOCATION);
        rows_inserted = rows_inserted + ALLOCATION;
        if(select_rows(file, db, table, schema, &field, next_insert_start, SELECT, rows_inserted) == -1){
            exit(EXIT_FAILURE);
        };
        next_insert_start += ALLOCATION;
        printf("Blocks allocated: %"PRId64"\n", rows_inserted);
//        logger(LL_WARN, __func__, "File size: %llu, blocks count: %llu", pg_file_size(), deallocation);
        i++;
        test_end = time(NULL);
    }
    printf("Test time: %jd\n", test_end - test_start);
    fclose(file);
    db_drop();
}