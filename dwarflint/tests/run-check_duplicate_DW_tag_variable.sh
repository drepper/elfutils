#! /bin/sh
# Copyright (C) 2010 Red Hat, Inc.
# This file is part of Red Hat elfutils.
#
# Red Hat elfutils is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# Red Hat elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with Red Hat elfutils; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.
#
# Red Hat elfutils is an included package of the Open Invention Network.
# An included package of the Open Invention Network is a package for which
# Open Invention Network licensees cross-license their patents.  No patent
# license is granted, either expressly or impliedly, by designation as an
# included package.  Should you wish to participate in the Open Invention
# Network licensing program, please visit www.openinventionnetwork.com
# <http://www.openinventionnetwork.com>.

. $srcdir/../tests/test-subr.sh

srcdir=$srcdir/tests

testfiles crc7.ko.debug

testrun_compare ./dwarflint --check check_duplicate_DW_tag_variable crc7.ko.debug <<EOF
warning: .debug_line: offset 0x3c4: the include #6 \`XXXXXX' is not used.
warning: .debug_line: table 967: the include #6 \`XXXXXX' is not used.
error: .debug_line: table 967: sequence of opcodes not terminated with DW_LNE_end_sequence.
error: .debug_info: DIE 0x3d21: Redeclaration of variable 'console_printk', originally seen at DIE 37f3.
error: .debug_info: DIE 0x3d2e: Redeclaration of variable 'hex_asc', originally seen at DIE 380b.
error: .debug_info: DIE 0x3d41: Redeclaration of variable '__per_cpu_offset', originally seen at DIE 382e.
error: .debug_info: DIE 0x3d4e: Redeclaration of variable 'per_cpu__current_task', originally seen at DIE 383b.
error: .debug_info: DIE 0x3d5b: Redeclaration of variable 'pv_info', originally seen at DIE 3848.
error: .debug_info: DIE 0x3d69: Redeclaration of variable 'pv_time_ops', originally seen at DIE 3856.
error: .debug_info: DIE 0x3d77: Redeclaration of variable 'pv_cpu_ops', originally seen at DIE 3864.
error: .debug_info: DIE 0x3d85: Redeclaration of variable 'pv_irq_ops', originally seen at DIE 3872.
error: .debug_info: DIE 0x3d93: Redeclaration of variable 'pv_apic_ops', originally seen at DIE 3880.
error: .debug_info: DIE 0x3da1: Redeclaration of variable 'pv_mmu_ops', originally seen at DIE 388e.
error: .debug_info: DIE 0x3daf: Redeclaration of variable 'nr_cpu_ids', originally seen at DIE 389c.
error: .debug_info: DIE 0x3dbc: Redeclaration of variable 'cpu_online_mask', originally seen at DIE 38a9.
error: .debug_info: DIE 0x3dc9: Redeclaration of variable 'cpu_present_mask', originally seen at DIE 38bb.
error: .debug_info: DIE 0x3dd6: Redeclaration of variable 'cpu_bit_bitmap', originally seen at DIE 38de.
error: .debug_info: DIE 0x3de9: Redeclaration of variable 'cpu_callout_mask', originally seen at DIE 38fc.
error: .debug_info: DIE 0x3df6: Redeclaration of variable 'boot_cpu_data', originally seen at DIE 3909.
error: .debug_info: DIE 0x3e03: Redeclaration of variable 'per_cpu__cpu_info', originally seen at DIE 3916.
error: .debug_info: DIE 0x3e10: Redeclaration of variable 'per_cpu__irq_stack_union', originally seen at DIE 3923.
error: .debug_info: DIE 0x3e1e: Redeclaration of variable 'mmu_cr4_features', originally seen at DIE 3931.
error: .debug_info: DIE 0x3e2c: Redeclaration of variable 'per_cpu__kernel_stack', originally seen at DIE 393f.
error: .debug_info: DIE 0x3e39: Redeclaration of variable 'node_states', originally seen at DIE 395c.
error: .debug_info: DIE 0x3e47: Redeclaration of variable 'nr_online_nodes', originally seen at DIE 396a.
error: .debug_info: DIE 0x3e55: Redeclaration of variable 'page_group_by_mobility_disabled', originally seen at DIE 3978.
error: .debug_info: DIE 0x3e6d: Redeclaration of variable 'node_data', originally seen at DIE 3996.
error: .debug_info: DIE 0x3e7a: Redeclaration of variable 'ioport_resource', originally seen at DIE 39a3.
error: .debug_info: DIE 0x3e87: Redeclaration of variable 'x86_init', originally seen at DIE 39b0.
error: .debug_info: DIE 0x3e94: Redeclaration of variable 'smp_found_config', originally seen at DIE 39bd.
error: .debug_info: DIE 0x3ea1: Redeclaration of variable 'phys_cpu_present_map', originally seen at DIE 39ca.
error: .debug_info: DIE 0x3eae: Redeclaration of variable 'time_status', originally seen at DIE 39d7.
error: .debug_info: DIE 0x3ebb: Redeclaration of variable 'jiffies', originally seen at DIE 39e4.
error: .debug_info: DIE 0x3ec8: Redeclaration of variable 'timer_stats_active', originally seen at DIE 39f6.
error: .debug_info: DIE 0x3ed5: Redeclaration of variable 'acpi_noirq', originally seen at DIE 3a03.
error: .debug_info: DIE 0x3ee2: Redeclaration of variable 'acpi_disabled', originally seen at DIE 3a10.
error: .debug_info: DIE 0x3eef: Redeclaration of variable 'acpi_ht', originally seen at DIE 3a1d.
error: .debug_info: DIE 0x3efc: Redeclaration of variable 'acpi_pci_disabled', originally seen at DIE 3a2a.
error: .debug_info: DIE 0x3f09: Redeclaration of variable 'apic_verbosity', originally seen at DIE 3a37.
error: .debug_info: DIE 0x3f16: Redeclaration of variable 'disable_apic', originally seen at DIE 3a44.
error: .debug_info: DIE 0x3f23: Redeclaration of variable 'x2apic_phys', originally seen at DIE 3a51.
error: .debug_info: DIE 0x3f30: Redeclaration of variable 'apic', originally seen at DIE 3a5e.
error: .debug_info: DIE 0x3f3e: Redeclaration of variable 'per_cpu__x86_bios_cpu_apicid', originally seen at DIE 3a72.
error: .debug_info: DIE 0x3f4b: Redeclaration of variable 'per_cpu__cpu_sibling_map', originally seen at DIE 3a80.
error: .debug_info: DIE 0x3f58: Redeclaration of variable 'per_cpu__cpu_core_map', originally seen at DIE 3a8d.
error: .debug_info: DIE 0x3f65: Redeclaration of variable 'per_cpu__cpu_number', originally seen at DIE 3a9a.
error: .debug_info: DIE 0x3f72: Redeclaration of variable 'smp_ops', originally seen at DIE 3aa7.
error: .debug_info: DIE 0x3f7f: Redeclaration of variable 'memnode', originally seen at DIE 3ab4.
error: .debug_info: DIE 0x3f8c: Redeclaration of variable 'mem_section', originally seen at DIE 3ad8.
error: .debug_info: DIE 0x3f9a: Redeclaration of variable 'per_cpu__x86_cpu_to_node_map', originally seen at DIE 3ae6.
error: .debug_info: DIE 0x3fa7: Redeclaration of variable 'x86_cpu_to_node_map_early_ptr', originally seen at DIE 3af3.
error: .debug_info: DIE 0x3fb4: Redeclaration of variable 'per_cpu__node_number', originally seen at DIE 3b00.
error: .debug_info: DIE 0x3fc1: Redeclaration of variable 'node_to_cpumask_map', originally seen at DIE 3b24.
error: .debug_info: DIE 0x3fce: Redeclaration of variable 'gfp_allowed_mask', originally seen at DIE 3b31.
error: .debug_info: DIE 0x3fdc: Redeclaration of variable '__tracepoint_kmalloc', originally seen at DIE 3b3f.
error: .debug_info: DIE 0x3fe9: Redeclaration of variable '__tracepoint_kmem_cache_alloc', originally seen at DIE 3b4c.
error: .debug_info: DIE 0x3ff6: Redeclaration of variable '__tracepoint_kmalloc_node', originally seen at DIE 3b59.
error: .debug_info: DIE 0x4003: Redeclaration of variable '__tracepoint_kmem_cache_alloc_node', originally seen at DIE 3b66.
error: .debug_info: DIE 0x4010: Redeclaration of variable '__tracepoint_kfree', originally seen at DIE 3b73.
error: .debug_info: DIE 0x401d: Redeclaration of variable '__tracepoint_kmem_cache_free', originally seen at DIE 3b80.
error: .debug_info: DIE 0x402a: Redeclaration of variable '__tracepoint_mm_page_free_direct', originally seen at DIE 3b8d.
error: .debug_info: DIE 0x4037: Redeclaration of variable '__tracepoint_mm_pagevec_free', originally seen at DIE 3b9a.
error: .debug_info: DIE 0x4044: Redeclaration of variable '__tracepoint_mm_page_alloc', originally seen at DIE 3ba7.
error: .debug_info: DIE 0x4052: Redeclaration of variable '__tracepoint_mm_page_alloc_zone_locked', originally seen at DIE 3bb5.
error: .debug_info: DIE 0x4060: Redeclaration of variable '__tracepoint_mm_page_pcpu_drain', originally seen at DIE 3bc3.
error: .debug_info: DIE 0x406e: Redeclaration of variable '__tracepoint_mm_page_alloc_extfrag', originally seen at DIE 3bd1.
error: .debug_info: DIE 0x407c: Redeclaration of variable 'kmalloc_caches', originally seen at DIE 3bef.
error: .debug_info: DIE 0x4089: Redeclaration of variable '__tracepoint_module_load', originally seen at DIE 3bfc.
error: .debug_info: DIE 0x4096: Redeclaration of variable '__tracepoint_module_free', originally seen at DIE 3c09.
error: .debug_info: DIE 0x40a3: Redeclaration of variable '__tracepoint_module_get', originally seen at DIE 3c16.
error: .debug_info: DIE 0x40b0: Redeclaration of variable '__tracepoint_module_put', originally seen at DIE 3c23.
error: .debug_info: DIE 0x40bd: Redeclaration of variable '__tracepoint_module_request', originally seen at DIE 3c30.
error: .debug_info: DIE 0x40ca: Found definition of variable 'crc7_syndrome_table' whose declaration was seen at DIE 3c4d.
error: .debug_info: DIE 0x7cf9: Redeclaration of variable 'console_printk', originally seen at DIE 7831.
error: .debug_info: DIE 0x7d06: Redeclaration of variable 'hex_asc', originally seen at DIE 7849.
error: .debug_info: DIE 0x7d19: Redeclaration of variable '__per_cpu_offset', originally seen at DIE 786c.
error: .debug_info: DIE 0x7d26: Redeclaration of variable 'per_cpu__current_task', originally seen at DIE 7879.
error: .debug_info: DIE 0x7d33: Redeclaration of variable 'pv_info', originally seen at DIE 7886.
error: .debug_info: DIE 0x7d41: Redeclaration of variable 'pv_time_ops', originally seen at DIE 7894.
error: .debug_info: DIE 0x7d4f: Redeclaration of variable 'pv_cpu_ops', originally seen at DIE 78a2.
error: .debug_info: DIE 0x7d5d: Redeclaration of variable 'pv_irq_ops', originally seen at DIE 78b0.
error: .debug_info: DIE 0x7d6b: Redeclaration of variable 'pv_apic_ops', originally seen at DIE 78be.
error: .debug_info: DIE 0x7d79: Redeclaration of variable 'pv_mmu_ops', originally seen at DIE 78cc.
error: .debug_info: DIE 0x7d87: Redeclaration of variable 'nr_cpu_ids', originally seen at DIE 78da.
error: .debug_info: DIE 0x7d94: Redeclaration of variable 'cpu_online_mask', originally seen at DIE 78e7.
error: .debug_info: DIE 0x7da1: Redeclaration of variable 'cpu_present_mask', originally seen at DIE 78f9.
error: .debug_info: DIE 0x7dae: Redeclaration of variable 'cpu_bit_bitmap', originally seen at DIE 791c.
error: .debug_info: DIE 0x7dc1: Redeclaration of variable 'cpu_callout_mask', originally seen at DIE 793a.
error: .debug_info: DIE 0x7dce: Redeclaration of variable 'boot_cpu_data', originally seen at DIE 7947.
error: .debug_info: DIE 0x7ddb: Redeclaration of variable 'per_cpu__cpu_info', originally seen at DIE 7954.
error: .debug_info: DIE 0x7de8: Redeclaration of variable 'per_cpu__irq_stack_union', originally seen at DIE 7961.
error: .debug_info: DIE 0x7df6: Redeclaration of variable 'mmu_cr4_features', originally seen at DIE 796f.
error: .debug_info: DIE 0x7e04: Redeclaration of variable 'per_cpu__kernel_stack', originally seen at DIE 797d.
error: .debug_info: DIE 0x7e11: Redeclaration of variable 'node_states', originally seen at DIE 799a.
error: .debug_info: DIE 0x7e1f: Redeclaration of variable 'nr_online_nodes', originally seen at DIE 79a8.
error: .debug_info: DIE 0x7e2d: Redeclaration of variable 'page_group_by_mobility_disabled', originally seen at DIE 79b6.
error: .debug_info: DIE 0x7e45: Redeclaration of variable 'node_data', originally seen at DIE 79d4.
error: .debug_info: DIE 0x7e52: Redeclaration of variable 'ioport_resource', originally seen at DIE 79e1.
error: .debug_info: DIE 0x7e5f: Redeclaration of variable 'x86_init', originally seen at DIE 79ee.
error: .debug_info: DIE 0x7e6c: Redeclaration of variable 'smp_found_config', originally seen at DIE 79fb.
error: .debug_info: DIE 0x7e79: Redeclaration of variable 'phys_cpu_present_map', originally seen at DIE 7a08.
error: .debug_info: DIE 0x7e86: Redeclaration of variable 'time_status', originally seen at DIE 7a15.
error: .debug_info: DIE 0x7e93: Redeclaration of variable 'jiffies', originally seen at DIE 7a22.
error: .debug_info: DIE 0x7ea0: Redeclaration of variable 'timer_stats_active', originally seen at DIE 7a34.
error: .debug_info: DIE 0x7ead: Redeclaration of variable 'acpi_noirq', originally seen at DIE 7a41.
error: .debug_info: DIE 0x7eba: Redeclaration of variable 'acpi_disabled', originally seen at DIE 7a4e.
error: .debug_info: DIE 0x7ec7: Redeclaration of variable 'acpi_ht', originally seen at DIE 7a5b.
error: .debug_info: DIE 0x7ed4: Redeclaration of variable 'acpi_pci_disabled', originally seen at DIE 7a68.
error: .debug_info: DIE 0x7ee1: Redeclaration of variable 'apic_verbosity', originally seen at DIE 7a75.
error: .debug_info: DIE 0x7eee: Redeclaration of variable 'disable_apic', originally seen at DIE 7a82.
error: .debug_info: DIE 0x7efb: Redeclaration of variable 'x2apic_phys', originally seen at DIE 7a8f.
error: .debug_info: DIE 0x7f08: Redeclaration of variable 'apic', originally seen at DIE 7a9c.
error: .debug_info: DIE 0x7f16: Redeclaration of variable 'per_cpu__x86_bios_cpu_apicid', originally seen at DIE 7ab0.
error: .debug_info: DIE 0x7f23: Redeclaration of variable 'per_cpu__cpu_sibling_map', originally seen at DIE 7abe.
error: .debug_info: DIE 0x7f30: Redeclaration of variable 'per_cpu__cpu_core_map', originally seen at DIE 7acb.
error: .debug_info: DIE 0x7f3d: Redeclaration of variable 'per_cpu__cpu_number', originally seen at DIE 7ad8.
error: .debug_info: DIE 0x7f4a: Redeclaration of variable 'smp_ops', originally seen at DIE 7ae5.
error: .debug_info: DIE 0x7f57: Redeclaration of variable 'memnode', originally seen at DIE 7af2.
error: .debug_info: DIE 0x7f64: Redeclaration of variable 'mem_section', originally seen at DIE 7b16.
error: .debug_info: DIE 0x7f72: Redeclaration of variable 'per_cpu__x86_cpu_to_node_map', originally seen at DIE 7b24.
error: .debug_info: DIE 0x7f7f: Redeclaration of variable 'x86_cpu_to_node_map_early_ptr', originally seen at DIE 7b31.
error: .debug_info: DIE 0x7f8c: Redeclaration of variable 'per_cpu__node_number', originally seen at DIE 7b3e.
error: .debug_info: DIE 0x7f99: Redeclaration of variable 'node_to_cpumask_map', originally seen at DIE 7b62.
error: .debug_info: DIE 0x7fa6: Redeclaration of variable 'gfp_allowed_mask', originally seen at DIE 7b6f.
error: .debug_info: DIE 0x7fb4: Redeclaration of variable '__tracepoint_kmalloc', originally seen at DIE 7b7d.
error: .debug_info: DIE 0x7fc1: Redeclaration of variable '__tracepoint_kmem_cache_alloc', originally seen at DIE 7b8a.
error: .debug_info: DIE 0x7fce: Redeclaration of variable '__tracepoint_kmalloc_node', originally seen at DIE 7b97.
error: .debug_info: DIE 0x7fdb: Redeclaration of variable '__tracepoint_kmem_cache_alloc_node', originally seen at DIE 7ba4.
error: .debug_info: DIE 0x7fe8: Redeclaration of variable '__tracepoint_kfree', originally seen at DIE 7bb1.
error: .debug_info: DIE 0x7ff5: Redeclaration of variable '__tracepoint_kmem_cache_free', originally seen at DIE 7bbe.
error: .debug_info: DIE 0x8002: Redeclaration of variable '__tracepoint_mm_page_free_direct', originally seen at DIE 7bcb.
error: .debug_info: DIE 0x800f: Redeclaration of variable '__tracepoint_mm_pagevec_free', originally seen at DIE 7bd8.
error: .debug_info: DIE 0x801c: Redeclaration of variable '__tracepoint_mm_page_alloc', originally seen at DIE 7be5.
error: .debug_info: DIE 0x802a: Redeclaration of variable '__tracepoint_mm_page_alloc_zone_locked', originally seen at DIE 7bf3.
error: .debug_info: DIE 0x8038: Redeclaration of variable '__tracepoint_mm_page_pcpu_drain', originally seen at DIE 7c01.
error: .debug_info: DIE 0x8046: Redeclaration of variable '__tracepoint_mm_page_alloc_extfrag', originally seen at DIE 7c0f.
error: .debug_info: DIE 0x8054: Redeclaration of variable 'kmalloc_caches', originally seen at DIE 7c2d.
error: .debug_info: DIE 0x8061: Redeclaration of variable '__tracepoint_module_load', originally seen at DIE 7c3a.
error: .debug_info: DIE 0x806e: Redeclaration of variable '__tracepoint_module_free', originally seen at DIE 7c47.
error: .debug_info: DIE 0x807b: Redeclaration of variable '__tracepoint_module_get', originally seen at DIE 7c54.
error: .debug_info: DIE 0x8088: Redeclaration of variable '__tracepoint_module_put', originally seen at DIE 7c61.
error: .debug_info: DIE 0x8095: Redeclaration of variable '__tracepoint_module_request', originally seen at DIE 7c6e.
EOF
