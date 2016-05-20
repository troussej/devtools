#!/bin/bash

input_file=$1

template=$2
out=$3

echo "" > $out

while IFS=, read skuId 
do
	sed -e "s/skuId/${skuId}/g" $template >> $out
done < ${input_file}