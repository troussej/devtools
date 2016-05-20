#!/bin/bash

function chunk(){
	echo $1"-"$2 | sed -e 's/ /-/g' | awk '{print tolower($0)}'
}

input_file=$1;
output_file=$2

properties=""

#get the property names from the header
IFS=',' read -r -a header < $input_file;

echo "" > $output_file

#print the values
sed 1d $input_file | while IFS=, read index productId name
do
	chunk "$name" $productId >> $output_file
done