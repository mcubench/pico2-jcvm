#include "gp_delete.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
int checks = 0;

struct FakeDeleteBackend {
  bool begin_ok = true;
  bool clear_applet_ok = true;
  bool clear_package_ok = true;
  bool sync_ok = true;
  bool rollback_ok = true;
  bool commit_ok = true;
  bool throw_on_mark = false;
  int begin_calls = 0;
  int applet_clear_calls = 0;
  int package_clear_calls = 0;
  int marker_calls = 0;
  int sync_calls = 0;
  int rollback_calls = 0;
  int commit_calls = 0;
  bool marked_package = false;
  uint16_t cleared_class = 0xffff;

  bool begin() { ++begin_calls; return begin_ok; }
  bool clear_applet(uint16_t class_index) {
    ++applet_clear_calls;
    cleared_class = class_index;
    return clear_applet_ok;
  }
  bool clear_package_static() {
    ++package_clear_calls;
    return clear_package_ok;
  }
  void mark_deleted(bool package) {
    ++marker_calls;
    marked_package = package;
    if (throw_on_mark) throw 1;
  }
  uint8_t sync() { ++sync_calls; return sync_ok ? 0 : 1; }
  bool rollback() { ++rollback_calls; return rollback_ok; }
  bool commit() { ++commit_calls; return commit_ok; }
};

void expect(bool condition, const char *name) {
  ++checks;
  if (!condition) {
    std::printf("TEST:FAIL %s\n", name);
    std::exit(EXIT_FAILURE);
  }
  std::printf("TEST:PASS %s\n", name);
}
} // namespace

int main() {
  constexpr std::array<uint8_t, 12> instance = {
      0x4f, 0x0a, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74, 0x31};
  constexpr std::array<uint8_t, 11> package = {
      0x4f, 0x09, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74};
  gp::DeleteRequest request;

  expect(gp::parse_delete_request(0x00, 0x00, instance.data(), instance.size(),
                                  request),
         "parse_instance_aid");
  expect(request.aid_length == 10 &&
             std::memcmp(request.aid, instance.data() + 2, 10) == 0 &&
             !request.delete_related_objects,
         "preserve_instance_target_and_flags");
  expect(gp::parse_delete_request(0x00, 0x80, package.data(), package.size(),
                                  request) &&
             request.aid_length == 9 && request.delete_related_objects,
         "parse_package_delete_related_flag");

  expect(!gp::parse_delete_request(0x80, 0x00, instance.data(), instance.size(),
                                   request),
         "reject_chained_p1");
  expect(!gp::parse_delete_request(0x00, 0x01, instance.data(), instance.size(),
                                   request),
         "reject_reserved_p2_bits");
  expect(!gp::parse_delete_request(0x00, 0x00, nullptr, 0, request),
         "reject_missing_data");

  constexpr std::array<uint8_t, 7> short_aid = {0x4f, 0x05, 1, 2, 3, 4, 5};
  expect(gp::parse_delete_request(0x00, 0x00, short_aid.data(),
                                  short_aid.size(), request),
         "accept_minimum_aid_length");
  constexpr std::array<uint8_t, 6> undersized_aid = {0x4f, 0x04, 1, 2, 3, 4};
  expect(!gp::parse_delete_request(0x00, 0x00, undersized_aid.data(),
                                   undersized_aid.size(), request),
         "reject_short_aid");

  constexpr std::array<uint8_t, 13> trailing_data = {
      0x4f, 0x0a, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74,
      0x31, 0x00};
  expect(!gp::parse_delete_request(0x00, 0x00, trailing_data.data(),
                                   trailing_data.size(), request),
         "reject_trailing_bytes");
  constexpr std::array<uint8_t, 12> truncated = {
      0x4f, 0x0b, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74, 0x31};
  expect(!gp::parse_delete_request(0x00, 0x00, truncated.data(),
                                   truncated.size(), request),
         "reject_truncated_tlv");
  constexpr std::array<uint8_t, 12> nonminimal_length = {
      0x4f, 0x81, 0x0a, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74};
  expect(!gp::parse_delete_request(0x00, 0x00, nonminimal_length.data(),
                                   nonminimal_length.size(), request),
         "reject_nonminimal_length");

  constexpr uint8_t package_aid[] = {0x4a, 0x43, 0x41, 0x6c, 0x67,
                                     0x54, 0x65, 0x73, 0x74};
  constexpr uint8_t instance_aid[] = {0x4a, 0x43, 0x41, 0x6c, 0x67,
                                     0x54, 0x65, 0x73, 0x74, 0x31};
  constexpr uint8_t unknown_aid[] = {0xa0, 0x00, 0x00, 0x01, 0x51};
  gp::DeleteTarget target;
  expect(gp::resolve_delete_target(true, true, instance_aid,
                                   sizeof(instance_aid), package_aid,
                                   sizeof(package_aid), instance_aid,
                                   sizeof(instance_aid), false, target) ==
                 0x9000 &&
             !target.package,
         "resolve_instance_delete_target");
  expect(gp::resolve_delete_target(true, true, package_aid,
                                   sizeof(package_aid), package_aid,
                                   sizeof(package_aid), instance_aid,
                                   sizeof(instance_aid), false, target) ==
             0x6985,
         "block_package_delete_with_installed_instance");
  expect(gp::resolve_delete_target(true, true, package_aid,
                                   sizeof(package_aid), package_aid,
                                   sizeof(package_aid), instance_aid,
                                   sizeof(instance_aid), true, target) ==
                 0x9000 &&
             target.package,
         "resolve_related_package_delete_target");
  expect(gp::resolve_delete_target(true, true, unknown_aid,
                                   sizeof(unknown_aid), package_aid,
                                   sizeof(package_aid), instance_aid,
                                   sizeof(instance_aid), true, target) ==
                 0x6a88 &&
             !target.package,
         "unknown_aid_has_no_delete_target");
  expect(gp::resolve_delete_target(false, false, package_aid,
                                   sizeof(package_aid), package_aid,
                                   sizeof(package_aid), instance_aid,
                                   sizeof(instance_aid), true, target) ==
             0x6a88,
         "unloaded_package_has_no_delete_target");

  FakeDeleteBackend instance_backend;
  expect(gp::commit_delete_state(instance_backend, false, true, 0x1234) ==
             0x9000,
         "commit_instance_delete_state");
  expect(instance_backend.applet_clear_calls == 1 &&
             instance_backend.package_clear_calls == 0 &&
             !instance_backend.marked_package &&
             instance_backend.sync_calls == 1 &&
             instance_backend.commit_calls == 1 &&
             instance_backend.rollback_calls == 0 &&
             instance_backend.cleared_class == 0x1234,
         "instance_delete_transaction_order");

  FakeDeleteBackend package_backend;
  expect(gp::commit_delete_state(package_backend, true, true, 0x4321) ==
             0x9000,
         "commit_package_delete_state");
  expect(package_backend.applet_clear_calls == 1 &&
             package_backend.package_clear_calls == 1 &&
             package_backend.marked_package &&
             package_backend.commit_calls == 1,
         "package_delete_clears_applet_and_static_state");

  FakeDeleteBackend missing_instance_backend;
  missing_instance_backend.begin_ok = false;
  expect(gp::commit_delete_state(missing_instance_backend, false, true,
                                 0x1234) == 0x6581 &&
             missing_instance_backend.rollback_calls == 0 &&
             missing_instance_backend.commit_calls == 0,
         "begin_failure_stops_without_rollback");

  FakeDeleteBackend clear_failure_backend;
  clear_failure_backend.clear_applet_ok = false;
  expect(gp::commit_delete_state(clear_failure_backend, false, true,
                                 0x1234) == 0x6581 &&
             clear_failure_backend.rollback_calls == 1 &&
             clear_failure_backend.sync_calls == 1 &&
             clear_failure_backend.commit_calls == 0,
         "applet_clear_failure_rolls_back_and_syncs");

  FakeDeleteBackend package_clear_failure_backend;
  package_clear_failure_backend.clear_package_ok = false;
  expect(gp::commit_delete_state(package_clear_failure_backend, true, false,
                                 0x4321) == 0x6581 &&
             package_clear_failure_backend.applet_clear_calls == 0 &&
             package_clear_failure_backend.rollback_calls == 1,
         "package_static_clear_failure_rolls_back");

  FakeDeleteBackend sync_failure_backend;
  sync_failure_backend.sync_ok = false;
  expect(gp::commit_delete_state(sync_failure_backend, false, false,
                                 0x1234) == 0x6581 &&
             sync_failure_backend.rollback_calls == 1 &&
             sync_failure_backend.sync_calls == 2 &&
             sync_failure_backend.commit_calls == 0,
         "snapshot_sync_failure_rolls_back_then_flushes");

  FakeDeleteBackend commit_failure_backend;
  commit_failure_backend.commit_ok = false;
  expect(gp::commit_delete_state(commit_failure_backend, false, false,
                                 0x1234) == 0x6581 &&
             commit_failure_backend.rollback_calls == 0 &&
             commit_failure_backend.commit_calls == 1,
         "snapshot_commit_failure_returns_storage_error");

  FakeDeleteBackend marker_exception_backend;
  marker_exception_backend.throw_on_mark = true;
  expect(gp::commit_delete_state(marker_exception_backend, false, false,
                                 0x1234) == 0x6581 &&
             marker_exception_backend.rollback_calls == 1 &&
             marker_exception_backend.sync_calls == 1,
         "marker_exception_rolls_back");

  std::printf("TEST:SUMMARY pass=%d fail=0\n", checks);
  return EXIT_SUCCESS;
}
