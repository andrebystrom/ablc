int fib (int a) {
    if (a <= 2) {
        return 1;
    }
    int a1 = a - 1;
    int a2 = a - 2;
    int a3 = fib(a1);
    int a4 = fib(a2);
    return a3 + a4;
}

void main() {
    int res = fib(15);
    print(res);
}