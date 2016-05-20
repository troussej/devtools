#!/bin/sh

#usage :
# $ genRepoItem.sh inFile outFile

# inFile is a CSV of form

# itemDescriptor,property1,property2 ....
# id1,value11,value21
# id2,value21,value22

function printLine(){
	echo "<set-property name=\"$1\">$2</set-property>";
}

function printAddItem(){
	echo "<add-item item-descriptor=\"$1\" id=\"$2\">";
}

function printClose(){
	echo "</add-item>";
}




input_file=$1;

properties=""

#get the property names from the header
IFS=',' read -r -a header < $input_file;

for index in ${!header[@]}

do
	key=${header[index]}		
	if [ $index -eq 0 ]
		then
		itemDescriptor=$key
	fi
	properties="$properties $key"
done

#print the values
sed 1d $input_file | while IFS=, read $properties
do
	for key in $properties
	do
		if [ $key = $itemDescriptor ]
			then
				printAddItem $itemDescriptor ${!key}
			else
				printLine $key ${!key};
		fi
	done
	printClose;
done

