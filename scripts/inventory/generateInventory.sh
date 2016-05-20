#!/bin/sh

HEADER_IMPORT_FILE="import_file_header.xml";
FOOTER_IMPORT_FILE="import_file_footer.xml";
WORK_PREFIX="part-";

warehouses="wh000002_huaqiao wh000001_yanjiao wh000003_dongguan"

input_file=$1;
output_file=$2;

> ${output_file}
> ${output_file}tmp

#basic version : set stock 100 to all skus

while IFS=, read skuId stock
do

	if [ -z "$stock" ]
	then
		stock="100";
	fi


	for warehouseId in $warehouses
	do

		invId=${skuId}"_"${warehouseId};
		echo '<add-item item-descriptor="inventory" id="'${invId}'" repository="/atg/commerce/inventory/InventoryRepository"> <set-property name="displayName"><![CDATA['${invId}']]></set-property> <set-property name="catalogRefId"><![CDATA['${skuId}']]></set-property> <set-property name="locationId"><![CDATA['${warehouseId}']]></set-property> <set-property name="stockLevel"><![CDATA['${stock}']]></set-property></add-item>' >> ${output_file}tmp;
	
	done
done < ${input_file}

lineNumber=$(cat ${output_file}tmp |wc -l);
echo 'there are' $lineNumber 'entries';
if test $lineNumber -gt $3
then
	echo 'The file will be splitted';
	# Perform the split
	output_file_prefix=${output_file%%.*};
	output_file_extension=${output_file##*.};
	split -d -l $3 ${output_file}tmp ${WORK_PREFIX};
	# Adding the header and footer to each split import file
	for work_file in ${WORK_PREFIX}*;
		do
		  result_file="${output_file_prefix}-${work_file}.${output_file_extension}";
		  cat ${HEADER_IMPORT_FILE} ${work_file} ${FOOTER_IMPORT_FILE} > ${result_file};
		done
	else
		cat ${HEADER_IMPORT_FILE} ${output_file}tmp ${FOOTER_IMPORT_FILE} > ${output_file};
fi

rm ${output_file}tmp;
