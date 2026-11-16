// SPDX-License-Identifier: MIT
// REPORT #1644: this C++ program is a TRACE WORKSHEET, not a kernel reproducer.
// Original C reproducer from the reporter is in upstream_repro.c.
// TEST INPUTS: from application kprobes, NOT from the reporter's public repro.c.
// io_poll_issue entry raw: 0x1, 0x22, 0x43, 0x64, 0x80, 0x80
// io_poll_issue return: -3072, -3072, -3072, -3072, -3072, -11
// +56us: io_poll_wake shows raw=0x40000080.
// TODO results remain unknown until calculated by trainee.
// RARE BUG HUNT: the CQE buffer ID lives in cqe.flags, not cqe.res;
// confusing those fields recycles the wrong user-space buffer.
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string_view>
struct Snapshot { std::string_view name; std::uint32_t raw; int outcome; };
struct Timepoint { int microseconds; std::string_view function; std::uint32_t raw; bool was_measured; };
constexpr std::uint32_t kRetryFlag=0x40000000u;
constexpr std::uint32_t kRefMask=0x3fffffffu;
constexpr std::uint32_t kBias=128u;
constexpr std::array<Snapshot,6> kEntries{{
 {"io_poll_issue",0x01u,-3072},
 {"io_poll_issue",0x22u,-3072},
 {"io_poll_issue",0x43u,-3072},
 {"io_poll_issue",0x64u,-3072},
 {"io_poll_issue",0x80u,-3072},
 {"io_poll_issue",0x80u,-11}
}};
constexpr std::array<Timepoint,5> kEvents{{
 {0, "io_poll_issue", 0x80u, true},
 {27, "io_poll_wake", 0x80u, true},
 {56, "io_poll_wake", 0x40000080u, true},
 {93, "io_poll_wake", 0x40000080u, true},
 {96, "io_poll_issue returned -EAGAIN", 0u, false}
}};
std::optional<std::uint32_t> decode_reference_count(std::uint32_t raw) {
 // TODO A: numeric value of raw reference bits, using kernel's mask.
 (void)raw; return std::nullopt;
}
std::optional<bool> decode_retry_bit(std::uint32_t raw) {
 // TODO B: distinguish retry bit from reference-count bits.
 (void)raw; return std::nullopt;
}
std::optional<std::uint32_t> release_reference(std::uint32_t raw,std::uint32_t amount) {
 // TODO C: compute unsigned 32-bit result of the recorded atomic subtraction.
 (void)raw; (void)amount; return std::nullopt;
}
std::optional<bool> loop_condition(std::uint32_t after_release) {
 // TODO D: reproduce the original source test, NOT a proposed fix.
 (void)after_release; return std::nullopt;
}
int main() {
 std::cout<<"ISSUE=1644 EXERCISE_ONLY; not the actual C reproducer\n";
 std::cout<<"BIAS="<<kBias<<" FLAG=0x"<<std::hex<<kRetryFlag
          <<" MASK=0x"<<kRefMask<<std::dec<<"\n";
 for (const Snapshot &s:kEntries) {
  std::cout<<s.name<<" raw=0x"<<std::hex<<s.raw<<std::dec
           <<" result="<<s.outcome;
  auto c=decode_reference_count(s.raw);
  auto b=decode_retry_bit(s.raw);
  if(c && b) std::cout<<" count="<<*c<<" retry="<<*b;
  else std::cout<<" count=? retry=? TODO";
  std::cout<<"\n";
 }
 for(const Timepoint &t:kEvents){
  std::cout<<"+"<<t.microseconds<<"us "<<t.function;
  if(t.was_measured) std::cout<<" refs=0x"<<std::hex<<t.raw<<std::dec;
  else std::cout<<" refs=NOT_MEASURED";
  std::cout<<"\n";
 }
 const auto released=release_reference(0x40000080u,0x80u);
 const auto should_retry=released?loop_condition(*released):std::nullopt;
 if(released && should_retry)
   std::cout<<"AFTER_RELEASE=0x"<<std::hex<<*released<<std::dec
            <<" LOOP="<<*should_retry<<"\n";
 else std::cout<<"AFTER_RELEASE=? LOOP=? TODO\n";
 std::cout<<"STATUS=UNSOLVED; compilation does not prove correctness\n";
 return 0;
}
