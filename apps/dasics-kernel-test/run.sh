#!/bin/sh
# Keep module ownership and ring-buffer evidence local to this invocation.
selection=${1:-all}
if [ "$#" -gt 0 ]; then shift; fi
rounds=1
run_id=
for option in "$@"; do
    case "$option" in
        --rounds=1|--rounds=2) rounds=${option#*=} ;;
        --run-id=*) run_id=${option#*=} ;;
        *) printf 'KDASICS_ERROR invalid-option=%s\n' "$option" >&2; exit 2 ;;
    esac
done
case "$selection" in
    data-bound-refill|fault-recovery|maincall-service) suites=$selection ;;
    all) suites='data-bound-refill fault-recovery maincall-service' ;;
    *) printf 'KDASICS_ERROR invalid-suite=%s\n' "$selection" >&2; exit 2 ;;
esac
if [ -z "$run_id" ]; then
    run_id=$(od -An -N16 -tx1 /dev/urandom | tr -d ' \n')
fi
case "$run_id" in
    *[!0-9a-f]*|'') printf 'KDASICS_ERROR invalid-run-id\n' >&2; exit 2 ;;
esac
[ "${#run_id}" -eq 32 ] || { printf 'KDASICS_ERROR invalid-run-id\n' >&2; exit 2; }

# Alternate paths support dependency-stub checks without touching real modules.
module_dir=${KDASICS_MODULE_DIR:-/modules}
kmsg=${KDASICS_KMSG_PATH:-/dev/kmsg}
lock=${KDASICS_LOCK_DIR:-/tmp/dasics-kernel-test.lock}
temporary=
locked=0
active=0
owned_t=0
owned_u=0
suite=
round=0
base=

module_present()
{
    lsmod > "$temporary/modules" || return 2
    awk -v name="$1" '$1 == name { found=1 } END { exit !found }' "$temporary/modules"
}

unload_owned()
{
    cleanup_status=0
    for suffix in tmod utmod; do
        case "$suffix" in
            tmod) owned=$owned_t ;;
            utmod) owned=$owned_u ;;
        esac
        [ "$owned" -eq 1 ] || continue
        rmmod "${base}_${suffix}"
        command_status=$?
        if [ "$command_status" -eq 0 ]; then
            case "$suffix" in tmod) owned_t=0 ;; utmod) owned_u=0 ;; esac
        fi
        module_present "${base}_${suffix}"
        presence=$?
        present=0
        [ "$presence" -eq 0 ] && present=1
        printf 'KDASICS_STEP run=%s suite=%s round=%s action=unload module=%s status=%s present=%s\n' \
            "$run_id" "$suite" "$round" "${base}_${suffix}" "$command_status" "$present"
        if [ "$command_status" -ne 0 ]; then
            [ "$cleanup_status" -ne 0 ] || cleanup_status=$command_status
        elif [ "$presence" -ne 1 ]; then
            [ "$cleanup_status" -ne 0 ] || cleanup_status=1
        fi
    done
    return "$cleanup_status"
}

snapshot()
{
    begin="KDASICS_KERNEL_BEGIN run=$run_id suite=$suite round=$round"
    end="KDASICS_KERNEL_END run=$run_id suite=$suite round=$round"
    printf '<6>%s\n' "$end" >> "$kmsg" || return 1
    dmesg > "$temporary/dmesg" || return 1
    awk -v first="$begin" -v last="$end" '
        {
            content=$0
            sub(/^<[0-9]+>/, "", content)
            sub(/^\[[[:space:]]*[0-9]+\.[0-9]+\][[:space:]]*/, "", content)
            if (content == first) { begins++; active=1 }
            if (active) print
            if (content == last) { ends++; active=0 }
        }
        END { if (begins != 1 || ends != 1 || active) exit 1 }
    ' "$temporary/dmesg" > "$temporary/scoped"
    captured=$?
    printf 'KDASICS_KMSG_BEGIN run=%s suite=%s round=%s\n' "$run_id" "$suite" "$round"
    while IFS= read -r line; do
        printf 'KDASICS_KMSG run=%s suite=%s round=%s text=%s\n' "$run_id" "$suite" "$round" "$line"
    done < "$temporary/scoped"
    printf 'KDASICS_KMSG_END run=%s suite=%s round=%s status=%s\n' "$run_id" "$suite" "$round" "$captured"
    [ "$captured" -eq 0 ] || return "$captured"
    if grep -Eq '(^|[[:space:]])FAIL([[:space:]:]|$)|result=FAIL|check=FAIL|failed=[1-9]|Kernel panic|PANIC:|Oops|BUG:|HIT BAD TRAP|Internal error:|Unable to handle kernel' "$temporary/scoped"; then
        return 1
    fi
    return 0
}

finish_case()
{
    case_status=$1
    unload_owned
    cleanup_status=$?
    [ "$case_status" -ne 0 ] || case_status=$cleanup_status
    snapshot
    snapshot_status=$?
    [ "$case_status" -ne 0 ] || case_status=$snapshot_status
    outcome=PASS
    [ "$case_status" -eq 0 ] || outcome=FAIL
    printf 'KDASICS_CASE_END run=%s suite=%s round=%s status=%s result=%s\n' \
        "$run_id" "$suite" "$round" "$case_status" "$outcome"
    active=0
    return "$case_status"
}

finish()
{
    final_status=$?
    trap - 0 HUP INT TERM
    if [ "$active" -eq 1 ]; then
        finish_case "$final_status"
        case_status=$?
        [ "$final_status" -ne 0 ] || final_status=$case_status
    fi
    if [ -n "$temporary" ]; then rm -rf "$temporary"; fi
    if [ "$locked" -eq 1 ]; then
        if ! rmdir "$lock"; then
            [ "$final_status" -ne 0 ] || final_status=1
        fi
    fi
    outcome=PASS
    [ "$final_status" -eq 0 ] || outcome=FAIL
    printf 'KDASICS_RUN_END version=1 run=%s suite=%s rounds=%s status=%s result=%s\n' \
        "$run_id" "$selection" "$rounds" "$final_status" "$outcome"
    exit "$final_status"
}

printf 'KDASICS_RUN_BEGIN version=1 run=%s suite=%s rounds=%s\n' "$run_id" "$selection" "$rounds"
trap finish 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir "$lock" || exit 1
locked=1
temporary=$(mktemp -d /tmp/dasics-kernel-test.XXXXXX) || exit 1
[ -w "$kmsg" ] || { printf 'KDASICS_ERROR kmsg-not-writable\n' >&2; exit 1; }
dmesg > "$temporary/dmesg" || exit 1
if grep -Fq "KDASICS_KERNEL_BEGIN run=$run_id " "$temporary/dmesg"; then
    printf 'KDASICS_ERROR run-id-already-present\n' >&2
    exit 1
fi
for required in insmod rmmod lsmod awk grep; do
    command -v "$required" >/dev/null 2>&1 || exit 127
done

round=1
while [ "$round" -le "$rounds" ]; do
    for suite in $suites; do
        # Refuse all six pre-existing modules, even when only one suite runs.
        for other in data_bound_refill fault_recovery maincall_service; do
            for suffix in utmod tmod; do
                module_present "${other}_${suffix}"
                presence=$?
                if [ "$presence" -ne 1 ]; then
                    printf 'KDASICS_ERROR preoccupied-or-unreadable=%s\n' "${other}_${suffix}" >&2
                    exit 1
                fi
            done
        done
        base=$(printf '%s' "$suite" | tr '-' '_')
        [ -f "$module_dir/${base}_utmod.ko" ] && [ -f "$module_dir/${base}_tmod.ko" ] || exit 1
        printf 'KDASICS_CASE_BEGIN run=%s suite=%s round=%s modules_clear=1\n' "$run_id" "$suite" "$round"
        printf '<6>KDASICS_KERNEL_BEGIN run=%s suite=%s round=%s\n' "$run_id" "$suite" "$round" >> "$kmsg" || exit 1
        active=1
        owned_t=0
        owned_u=0
        insmod "$module_dir/${base}_utmod.ko" trust=0
        command_status=$?
        [ "$command_status" -ne 0 ] || owned_u=1
        module_present "${base}_utmod"
        presence=$?
        present=0
        [ "$presence" -eq 0 ] && present=1
        printf 'KDASICS_STEP run=%s suite=%s round=%s action=load module=%s trust=0 status=%s present=%s\n' \
            "$run_id" "$suite" "$round" "${base}_utmod" "$command_status" "$present"
        [ "$command_status" -eq 0 ] || exit "$command_status"
        [ "$presence" -eq 0 ] || exit 1
        insmod "$module_dir/${base}_tmod.ko" trust=1
        command_status=$?
        [ "$command_status" -ne 0 ] || owned_t=1
        module_present "${base}_tmod"
        presence=$?
        present=0
        [ "$presence" -eq 0 ] && present=1
        printf 'KDASICS_STEP run=%s suite=%s round=%s action=load module=%s trust=1 status=%s present=%s\n' \
            "$run_id" "$suite" "$round" "${base}_tmod" "$command_status" "$present"
        [ "$command_status" -eq 0 ] || exit "$command_status"
        [ "$presence" -eq 0 ] || exit 1
        finish_case 0
        case_status=$?
        [ "$case_status" -eq 0 ] || exit "$case_status"
    done
    round=$((round + 1))
done
exit 0
