#!/bin/bash

inFile=$1;
outFile=$2;
prefix=$3;

if [ -z "$prefix" ]
then
	prefix="SIT_";
fi


header="<import-items>";
footer="</import-items>";


cp $inFile ${outFile}_src

# recover all ids

grep -oP 'id="\K[^"]*' $inFile > idsTmp;

while IFS='' read -r line || [[ -n "$line" ]]; do
 #  echo "Text read from file: $line"
  sed  -ie "s/${line}/${prefix}${line}/g"  ${outFile}_src;
done < "idsTmp"

#split in two : create items then add relations

cp ${outFile}_src ${outFile}tmp;


#remove lines that refer to another item
#sed -i "/.*set-property.*${prefix}/d" ${outFile}tmp



#wrap with import-item tag to avoid issues with links

echo $header > ${outFile}
cat ${outFile}_src >> ${outFile}
echo $footer >> ${outFile}

rm ${outFile}tmp;
rm ${outFile}_src;
rm idsTmp;
