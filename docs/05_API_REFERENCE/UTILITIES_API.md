# Baa Utilities API Reference

This document provides a comprehensive API reference for the Baa language utility functions and data structures.

## 🎯 Overview

The Baa utilities provide essential data structures, memory management, string operations, and helper functions used throughout the compiler. These utilities are designed to handle Arabic text and Unicode properly.

## 📋 Core Data Structures

### BaaVector

Dynamic array implementation.

```c
typedef struct BaaVector {
    void** data;
    size_t size;
    size_t capacity;
    void (*destructor)(void*);
} BaaVector;
```

### BaaHashMap

Hash table implementation for key-value storage.

```c
typedef struct BaaHashMap {
    BaaHashMapEntry** buckets;
    size_t bucket_count;
    size_t size;
    size_t (*hash_func)(const void* key);
    bool (*key_equals)(const void* a, const void* b);
    void (*key_destructor)(void* key);
    void (*value_destructor)(void* value);
} BaaHashMap;
```

### BaaString

Unicode-aware string structure.

```c
typedef struct BaaString {
    char* data;
    size_t length;
    size_t capacity;
    BaaStringEncoding encoding;
} BaaString;
```

## 🧠 Memory Management

### Core Memory Functions

#### `baa_malloc`
```c
void* baa_malloc(size_t size);
```
**Description:** Allocates memory with error checking.
**Parameters:**
- `size`: Number of bytes to allocate
**Returns:** Pointer to allocated memory or NULL on failure

#### `baa_calloc`
```c
void* baa_calloc(size_t count, size_t size);
```
**Description:** Allocates zero-initialized memory.
**Parameters:**
- `count`: Number of elements
- `size`: Size of each element
**Returns:** Pointer to allocated memory or NULL on failure

#### `baa_realloc`
```c
void* baa_realloc(void* ptr, size_t new_size);
```
**Description:** Reallocates memory block.
**Parameters:**
- `ptr`: Existing memory pointer
- `new_size`: New size in bytes
**Returns:** Pointer to reallocated memory or NULL on failure

#### `baa_free`
```c
void baa_free(void* ptr);
```
**Description:** Frees allocated memory.
**Parameters:**
- `ptr`: Memory pointer to free

#### `baa_strdup`
```c
char* baa_strdup(const char* str);
```
**Description:** Duplicates a string with memory allocation.
**Parameters:**
- `str`: String to duplicate
**Returns:** Newly allocated string copy or NULL on failure

## 📊 Vector Operations

### Creation and Destruction

#### `baa_vector_create`
```c
BaaVector* baa_vector_create(void (*destructor)(void*));
```
**Description:** Creates a new vector.
**Parameters:**
- `destructor`: Function to free elements (can be NULL)
**Returns:** New vector instance or NULL on failure

#### `baa_vector_destroy`
```c
void baa_vector_destroy(BaaVector* vector);
```
**Description:** Destroys vector and frees all elements.
**Parameters:**
- `vector`: Vector to destroy

### Element Access

#### `baa_vector_get`
```c
void* baa_vector_get(BaaVector* vector, size_t index);
```
**Description:** Gets element at specified index.
**Parameters:**
- `vector`: Vector instance
- `index`: Element index
**Returns:** Element pointer or NULL if index out of bounds

#### `baa_vector_set`
```c
BaaResult baa_vector_set(BaaVector* vector, size_t index, void* element);
```
**Description:** Sets element at specified index.
**Parameters:**
- `vector`: Vector instance
- `index`: Element index
- `element`: Element to set
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_vector_push`
```c
BaaResult baa_vector_push(BaaVector* vector, void* element);
```
**Description:** Adds element to end of vector.
**Parameters:**
- `vector`: Vector instance
- `element`: Element to add
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_vector_pop`
```c
void* baa_vector_pop(BaaVector* vector);
```
**Description:** Removes and returns last element.
**Parameters:**
- `vector`: Vector instance
**Returns:** Last element or NULL if vector is empty

### Vector Properties

#### `baa_vector_size`
```c
size_t baa_vector_size(BaaVector* vector);
```
**Description:** Gets number of elements in vector.
**Parameters:**
- `vector`: Vector instance
**Returns:** Number of elements

#### `baa_vector_is_empty`
```c
bool baa_vector_is_empty(BaaVector* vector);
```
**Description:** Checks if vector is empty.
**Parameters:**
- `vector`: Vector instance
**Returns:** `true` if empty, `false` otherwise

## 🗂️ HashMap Operations

### Creation and Destruction

#### `baa_hashmap_create`
```c
BaaHashMap* baa_hashmap_create(
    size_t (*hash_func)(const void* key),
    bool (*key_equals)(const void* a, const void* b),
    void (*key_destructor)(void* key),
    void (*value_destructor)(void* value)
);
```
**Description:** Creates a new hash map.
**Parameters:**
- `hash_func`: Hash function for keys
- `key_equals`: Key comparison function
- `key_destructor`: Function to free keys (can be NULL)
- `value_destructor`: Function to free values (can be NULL)
**Returns:** New hash map instance or NULL on failure

#### `baa_hashmap_destroy`
```c
void baa_hashmap_destroy(BaaHashMap* map);
```
**Description:** Destroys hash map and frees all entries.
**Parameters:**
- `map`: Hash map to destroy

### Key-Value Operations

#### `baa_hashmap_put`
```c
BaaResult baa_hashmap_put(BaaHashMap* map, void* key, void* value);
```
**Description:** Inserts or updates key-value pair.
**Parameters:**
- `map`: Hash map instance
- `key`: Key to insert/update
- `value`: Value to associate with key
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_hashmap_get`
```c
void* baa_hashmap_get(BaaHashMap* map, const void* key);
```
**Description:** Gets value associated with key.
**Parameters:**
- `map`: Hash map instance
- `key`: Key to look up
**Returns:** Associated value or NULL if not found

#### `baa_hashmap_remove`
```c
bool baa_hashmap_remove(BaaHashMap* map, const void* key);
```
**Description:** Removes key-value pair.
**Parameters:**
- `map`: Hash map instance
- `key`: Key to remove
**Returns:** `true` if removed, `false` if not found

#### `baa_hashmap_contains`
```c
bool baa_hashmap_contains(BaaHashMap* map, const void* key);
```
**Description:** Checks if key exists in map.
**Parameters:**
- `map`: Hash map instance
- `key`: Key to check
**Returns:** `true` if key exists, `false` otherwise

## 🔤 String Operations

### String Creation

#### `baa_string_create`
```c
BaaString* baa_string_create(const char* initial_value);
```
**Description:** Creates a new string.
**Parameters:**
- `initial_value`: Initial string content (can be NULL)
**Returns:** New string instance or NULL on failure

#### `baa_string_create_with_capacity`
```c
BaaString* baa_string_create_with_capacity(size_t capacity);
```
**Description:** Creates string with specified initial capacity.
**Parameters:**
- `capacity`: Initial capacity in bytes
**Returns:** New string instance or NULL on failure

#### `baa_string_destroy`
```c
void baa_string_destroy(BaaString* string);
```
**Description:** Destroys string and frees memory.
**Parameters:**
- `string`: String to destroy

### String Manipulation

#### `baa_string_append`
```c
BaaResult baa_string_append(BaaString* string, const char* text);
```
**Description:** Appends text to string.
**Parameters:**
- `string`: String instance
- `text`: Text to append
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_string_append_char`
```c
BaaResult baa_string_append_char(BaaString* string, char c);
```
**Description:** Appends single character to string.
**Parameters:**
- `string`: String instance
- `c`: Character to append
**Returns:** `BAA_RESULT_SUCCESS` or error code

#### `baa_string_clear`
```c
void baa_string_clear(BaaString* string);
```
**Description:** Clears string content.
**Parameters:**
- `string`: String instance

### String Properties

#### `baa_string_length`
```c
size_t baa_string_length(BaaString* string);
```
**Description:** Gets string length in bytes.
**Parameters:**
- `string`: String instance
**Returns:** String length

#### `baa_string_c_str`
```c
const char* baa_string_c_str(BaaString* string);
```
**Description:** Gets null-terminated C string.
**Parameters:**
- `string`: String instance
**Returns:** C string pointer

## 🌍 Unicode and Arabic Support

### Unicode Utilities

#### `baa_utf8_char_length`
```c
int baa_utf8_char_length(const char* utf8_char);
```
**Description:** Gets length of UTF-8 character in bytes.
**Parameters:**
- `utf8_char`: UTF-8 character pointer
**Returns:** Character length (1-4 bytes) or -1 on error

#### `baa_utf8_validate`
```c
bool baa_utf8_validate(const char* str, size_t length);
```
**Description:** Validates UTF-8 string.
**Parameters:**
- `str`: String to validate
- `length`: String length in bytes
**Returns:** `true` if valid UTF-8, `false` otherwise

### Arabic Text Utilities

#### `baa_is_arabic_char`
```c
bool baa_is_arabic_char(uint32_t codepoint);
```
**Description:** Checks if Unicode codepoint is Arabic.
**Parameters:**
- `codepoint`: Unicode codepoint
**Returns:** `true` if Arabic character, `false` otherwise

#### `baa_is_arabic_digit`
```c
bool baa_is_arabic_digit(uint32_t codepoint);
```
**Description:** Checks if Unicode codepoint is Arabic-Indic digit.
**Parameters:**
- `codepoint`: Unicode codepoint
**Returns:** `true` if Arabic digit, `false` otherwise

#### `baa_arabic_digit_to_int`
```c
int baa_arabic_digit_to_int(uint32_t codepoint);
```
**Description:** Converts Arabic-Indic digit to integer.
**Parameters:**
- `codepoint`: Arabic digit codepoint
**Returns:** Integer value (0-9) or -1 if not Arabic digit

## 📝 Usage Examples

### Vector Usage

```c
// Create vector for strings
BaaVector* strings = baa_vector_create(free);

// Add elements
baa_vector_push(strings, baa_strdup("مرحبا"));
baa_vector_push(strings, baa_strdup("عالم"));

// Access elements
for (size_t i = 0; i < baa_vector_size(strings); i++) {
    char* str = (char*)baa_vector_get(strings, i);
    printf("String %zu: %s\n", i, str);
}

// Cleanup
baa_vector_destroy(strings);
```

### HashMap Usage

```c
// Create hash map for string->int mapping
BaaHashMap* map = baa_hashmap_create(
    baa_string_hash, baa_string_equals, free, free
);

// Add entries
int* value1 = baa_malloc(sizeof(int));
*value1 = 42;
baa_hashmap_put(map, baa_strdup("مفتاح"), value1);

// Lookup
int* result = (int*)baa_hashmap_get(map, "مفتاح");
if (result) {
    printf("Value: %d\n", *result);
}

// Cleanup
baa_hashmap_destroy(map);
```

## 🔗 Related APIs

- **[AST API](../02_COMPILER_ARCHITECTURE/AST.md)** - Uses utility functions
- **[Preprocessor API](PREPROCESSOR_API.md)** - Uses utility functions
- **[Lexer API](LEXER_API.md)** - Uses string and vector utilities

---

**For more information, see the [Architecture Overview](../02_COMPILER_ARCHITECTURE/ARCHITECTURE_OVERVIEW.md).**
