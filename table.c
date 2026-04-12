#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

struct Entry {
    EntryState state;
    Value key;
    Value value;
};

static uint32_t hashValue(Value key) {
    switch (key.type) {
        case VAL_NIL:
            return 1;

        case VAL_BOOL:
            return AS_BOOL(key) ? 2 : 3;

        case VAL_NUMBER: {
            union {
                double num;
                uint64_t bits;
            } data;
            data.num = AS_NUMBER(key);
            return (uint32_t)(data.bits ^ (data.bits >> 32));
        }

        case VAL_OBJ:
            if (IS_STRING(key)) return AS_STRING(key)->hash;
            return 0;
    }

    return 0;
}

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, Value key) {
    uint32_t index = hashValue(key) % capacity;
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];

        if (entry->state == ENTRY_EMPTY) {
            return tombstone != NULL ? tombstone : entry;
        }

        if (entry->state == ENTRY_TOMBSTONE) {
            if (tombstone == NULL) tombstone = entry;
        } else if (valuesEqual(entry->key, key)) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool tableGet(Table* table, Value key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->state != ENTRY_OCCUPIED) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);

    for (int i = 0; i < capacity; i++) {
        entries[i].state = ENTRY_EMPTY;
        entries[i].key = NIL_VAL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;

    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->state != ENTRY_OCCUPIED) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->state = ENTRY_OCCUPIED;
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, Value key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->state != ENTRY_OCCUPIED;

    if (isNewKey) {
        table->count++;
        entry->state = ENTRY_OCCUPIED;
        entry->key = key;
    }

    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, Value key) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->state != ENTRY_OCCUPIED) return false;

    entry->state = ENTRY_TOMBSTONE;
    entry->key = NIL_VAL;
    entry->value = NIL_VAL;
    return true;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->state == ENTRY_OCCUPIED) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;

    for (;;) {
        Entry* entry = &table->entries[index];

        if (entry->state == ENTRY_EMPTY) {
            return NULL;
        }

        if (entry->state == ENTRY_OCCUPIED &&
            IS_STRING(entry->key) &&
            AS_STRING(entry->key)->length == length &&
            AS_STRING(entry->key)->hash == hash &&
            memcmp(AS_STRING(entry->key)->chars, chars, length) == 0) {
            return AS_STRING(entry->key);
        }

        index = (index + 1) % table->capacity;
    }
}