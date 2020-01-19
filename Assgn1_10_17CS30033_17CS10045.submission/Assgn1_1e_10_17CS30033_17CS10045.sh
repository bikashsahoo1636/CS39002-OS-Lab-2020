echo "Path to input file? "
read path
echo "Column to read? "
read column
if [ 0 -ge "$column" ] || [ 5 -le "$column" ] ; then
    echo "Column number should be between 1 and 4, both inclusive"
    exit 1
fi

case $column in
    1)
        awk 'BEGIN{FS=OFS=" "} {print tolower($1),$2,$3,$4}' $path > 'output.txt'
        awk 'BEGIN{FS=OFS=" "} { vals[$1] = vals[$1] + 1} END { for(c in vals) { print c, vals[c]} }' 'output.txt' > '1e_output_'"$column"'_column.freq';;
    2)
        awk 'BEGIN{FS=OFS=" "} {print $1,tolower($2),$3,$4}' $path > 'output.txt'
        awk 'BEGIN{FS=OFS=" "} { vals[$2] = vals[$2] + 1} END { for(c in vals) { print c, vals[c]} }' 'output.txt' > '1e_output_'"$column"'_column.freq';;
    3)
        awk 'BEGIN{FS=OFS=" "} {print $1,$2,tolower($3),$4}' $path > 'output.txt'
        awk 'BEGIN{FS=OFS=" "} { vals[$3] = vals[$3] + 1} END { for(c in vals) { print c, vals[c]} }' 'output.txt' > '1e_output_'"$column"'_column.freq';;
    4)
        awk 'BEGIN{FS=OFS=" "} {print $1,$2,$3,tolower($4)}' $path > 'output.txt'
        awk 'BEGIN{FS=OFS=" "} { vals[$4] = vals[$4] + 1} END { for(c in vals) { print c, vals[c]} }' 'output.txt' > '1e_output_'"$column"'_column.freq';;
esac
mv output.txt $path
sort -r -k 2 '1e_output_'"$column"'_column.freq' -o '1e_output_'"$column"'_column.freq'
