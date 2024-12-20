struct push_constant_t {
    uint *a;
    uint *b;
}
[vk::push_constant] push_constant_t pc;
[shader("compute")]
[numthreads(1, 1, 1)]
void compute_main() {
    printf("value of a: %u\n", *pc.a);
    *pc.b = *pc.a;
}