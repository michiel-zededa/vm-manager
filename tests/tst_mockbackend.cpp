#include <QtTest>

#include "core/MockBackend.h"

using namespace vmm;

// Exercises the backend contract through the mock so regressions in the shared
// interface semantics are caught without needing a hypervisor. The libvirt
// backend is expected to satisfy the same behavioural contract.
class TstMockBackend : public QObject {
    Q_OBJECT
private slots:
    void opensWithSeedData();
    void lifecycleTransitions();
    void defineAndUndefine();
    void snapshotRoundTrip();
    void cloneCreatesIndependentVm();
};

void TstMockBackend::opensWithSeedData() {
    MockBackend b("qemu:///session", "Test", true);
    QVERIFY(!b.isOpen());
    b.open();
    QVERIFY(b.isOpen());
    const auto vms = b.listVms();
    QVERIFY(vms.size() >= 3);
    QVERIFY(!b.listStoragePools().isEmpty());
    QVERIFY(!b.listNetworks().isEmpty());
}

void TstMockBackend::lifecycleTransitions() {
    MockBackend b("qemu:///session", "Test", true);
    b.open();
    const QString uuid = b.listVms().first().uuid;

    b.start(uuid);
    QCOMPARE(b.vm(uuid).state, VmState::Running);
    b.pause(uuid);
    QCOMPARE(b.vm(uuid).state, VmState::Paused);
    b.resume(uuid);
    QCOMPARE(b.vm(uuid).state, VmState::Running);
    b.shutdown(uuid);
    QCOMPARE(b.vm(uuid).state, VmState::ShutOff);

    b.setAutostart(uuid, true);
    QVERIFY(b.vm(uuid).autostart);
}

void TstMockBackend::defineAndUndefine() {
    MockBackend b("qemu:///session", "Test", true);
    b.open();
    const int before = b.listVms().size();

    VmCreateRequest req;
    req.name = "unit-test-vm";
    req.vcpus = 3;
    req.memoryMiB = 1536;
    const VmInfo created = b.define(req);
    QCOMPARE(created.name, QString("unit-test-vm"));
    QCOMPARE(created.vcpus, 3);
    QCOMPARE(b.listVms().size(), before + 1);

    b.undefine(created.uuid, true);
    QCOMPARE(b.listVms().size(), before);
}

void TstMockBackend::snapshotRoundTrip() {
    MockBackend b("qemu:///session", "Test", true);
    b.open();
    const QString uuid = b.listVms().first().uuid;
    QVERIFY(b.listSnapshots(uuid).isEmpty());

    b.createSnapshot(uuid, "s1", "first", false);
    b.createSnapshot(uuid, "s2", "second", true);
    auto snaps = b.listSnapshots(uuid);
    QCOMPARE(snaps.size(), 2);
    QCOMPARE(snaps.last().name, QString("s2"));
    QVERIFY(snaps.last().isCurrent);

    b.deleteSnapshot(uuid, "s1");
    QCOMPARE(b.listSnapshots(uuid).size(), 1);
}

void TstMockBackend::cloneCreatesIndependentVm() {
    MockBackend b("qemu:///session", "Test", true);
    b.open();
    const VmInfo src = b.listVms().first();
    const VmInfo clone = b.clone(src.uuid, "clone-of-it", false);
    QVERIFY(clone.uuid != src.uuid);
    QCOMPARE(clone.name, QString("clone-of-it"));
    QCOMPARE(clone.state, VmState::ShutOff);
}

QTEST_MAIN(TstMockBackend)
#include "tst_mockbackend.moc"
