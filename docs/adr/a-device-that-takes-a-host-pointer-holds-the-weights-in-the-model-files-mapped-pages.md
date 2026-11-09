# A device that takes a host pointer holds the weights in the model file's mapped pages

## Context

A model's weights are most of a process's memory. Read from the file into buffers of the device, they are the
process's own memory, written once: macOS counts every byte in the process's footprint, which Activity Monitor
shows, and under memory pressure compresses them or writes them to swap. ggml's CPU and Metal devices take a host
pointer (`buffer_from_host_ptr`): a buffer can wrap memory the program gives, a read-only map of the file included,
whose pages belong to the system's file cache, which the system drops under pressure and reads again from the file.
Vulkan's device takes none.

Measured on an Apple M4 on 2026-11-09, `speech asr` of the same 60 s of Japanese speech, the two builds alternating in
five rounds, and in four for five requests in one process:

| haas-ja-v1.0 Q8_0, Metal | A copy | The mapped pages |
|---|---|---|
| Peak memory footprint | 788 to 789 MB | 162 to 166 MB |
| Load time, median | 0.32 s | 0.15 s |
| The first request, median | 1.10 s | 1.15 s |
| Five requests in one process, median | 6.69 s | 6.58 s |

The texts, and the times with `--timestamps`, are the same byte for byte, for haas-ja-v1.0 and parakeet-tdt-0.6b-v3 on
Metal and on the CPU, and for Silero VAD's regions. The first request reads the pages it touches from the file cache,
which the load no longer does.

## Decision

A model file's tensors on a device that takes a host pointer stay in the file's pages, mapped read-only, through a
buffer from that pointer for each run of the device's tensors that lie next to each other in the file, so that no buffer
covers another device's tensors, which Metal would count as memory in use. A device that takes none gets a buffer of its
own, read from the file in pieces. A copy of a loaded model onto another device reads the tensors from the device that
holds them, as before.

The alternative was turned down:

- A copy on every device keeps every weight in the process's memory, the footprint of a resident model five times its
  working memory, for a load that the map makes faster.

## Consequences

The file must stay unchanged while a model holds its pages: a program that replaces a model file writes the new one
beside it and renames it over the old one, which leaves the mapped file in place until the model is freed.
