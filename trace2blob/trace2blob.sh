#/bin/bash

if [ $# -ne 0 ]; then
    echo -e "This script reads an arbitrary amount of lines from stdio.\n"
    echo "Usage example: echo \"40, ['127.0.0.1', '127.0.0.2']\" | $0 > blobname.h"
fi

storage=$(mktemp)
last_timestamp=0
item_count=0
overall_packet_count=0

# Loop interrupts csv one per line.
while read line
do
    # Unpack the row entry.
    preprocessed=$(echo "$line" | tr -d "[]' \"\r")
    readarray -td, data <<<"$preprocessed,"
    timedelta=$((${data[0]} - $last_timestamp))
    last_timestamp=${data[0]}

    packet_count=0
    # Iterate packets for that interrupt row entry.
    for item in ${data[@]:1}; do
        # Increment packet count for interrupt
        ((packet_count++))
        ((overall_packet_count++))

        # Use last byte of ip as process identifier.
        proc_id=$(echo "$item" | sed -e "s/.*\.//")
    done

    # Save 4 bytes for the timedelta, 1 byte for the process identifier, 1 byte for the packet count.
    # NOTE: this is little endian
    printf "%.2x%.2x%.2x%.2x%.2x%.2x" \
        $(($timedelta & 0xff)) \
        $((($timedelta & 0xff00) >> 8)) \
        $((($timedelta & 0xff0000) >> 16)) \
        $((($timedelta & 0xff000000) >> 24)) \
        $(($proc_id & 0xff)) \
        $(($packet_count & 0xff)) | xxd -r -p >> $storage

    ((item_count++))
done < /dev/stdin

# Construct compressed C blob header.
echo "#ifndef __TRACE_BLOB__"
echo -e "#define __TRACE_BLOB__\n\n"
echo -e "#define TRACE_IRQ_COUNT $item_count"
echo -e "#define TRACE_PACKET_COUNT $overall_packet_count\n"

echo "const unsigned char trace[$((item_count * 6))] = {"

cat "$storage" | xxd -i

echo -e "};\n"
echo "#endif"
