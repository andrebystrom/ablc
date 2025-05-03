int fib (int a) {
    if (a <= 2) {
        return 1;
    }
    return fib(a - 1) + fib(a - 2);
}

void main() {
    print(fib(10));
}