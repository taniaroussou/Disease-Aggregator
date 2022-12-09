#include <stdlib.h>
#include <assert.h>
#include <iostream>
#define DEF_SIZE 10

using namespace std;

template <typename T> class Vector{
private:
    int size;
    int capacity;
    T *array;
public:
    Vector();
    ~Vector();
    bool pushBack(T );
    T getAt(int );
    void setAt(int , T );
    int getSize();
    void sortVector(int (*)(const void*, const void*));
};

template<typename T>
inline void Vector<T>::sortVector(int (*comp)(const void*, const void*)){
    qsort(array, size, sizeof(T), comp);
}

template<typename T>
inline Vector<T>::Vector(){
    array = (T*)malloc(sizeof(T) * DEF_SIZE);
    size = 0;
    capacity = DEF_SIZE;
}

template<typename T>
inline Vector<T>::~Vector(){
    free(array);
}

template<typename T>
inline bool Vector<T>::pushBack(T elem){
    if (size == capacity){
        capacity = 2*size;
        array = (T*)realloc(array, sizeof(T) * capacity);
        if (!array)
            return false;
    }
    array[size++] = elem;
    return true;
}

template<typename T>
inline T Vector<T>::getAt(int index){
    assert(index < size);
    assert(index >= 0);
    return array[index];
}

template<typename T>
inline void Vector<T>::setAt(int index, T elem){
    assert(index < size);
    assert(index >= 0);
    array[index] = elem;
}

template<typename T>
inline int Vector<T>::getSize(){
    return size;
}
