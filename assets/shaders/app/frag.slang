#include "common.slang"

struct fragment_t {
  float4 o_color : COLOR0;
};

[shader("fragment")]
fragment_t fragment_main(per_fragment_t per_fragment) : SV_Target {
  fragment_t fragment;

  fragment.o_color = float4(1, 1, 1, 1);

  return fragment;
}
