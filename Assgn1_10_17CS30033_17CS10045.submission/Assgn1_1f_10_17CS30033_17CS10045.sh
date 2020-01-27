path="1f.graph.edgelist"
# removing parallel edges
sort -k1 $path | uniq > output.txt
# removing self edges
awk 'BEGIN{FS=OFS=" "} {if($1!=$2) print $1,$2}' output.txt > $path
rm output.txt
# removing mirror edges
awk 'BEGIN{FS=OFS=" "} { if (vals[$1$2] != 1 && vals[$2$1] != 1) {print $1, $2; vals[$1$2]=1;} }' $path > '1f_output_graph.edgelist'
# counting edges for each
awk 'BEGIN{FS=OFS=" "} { vals[$1] = vals[$1] + 1; vals[$2] = vals[$2] + 1} END { for(c in vals) { print c, vals[c]} }' '1f_output_graph.edgelist' | sort -r -k2 | head -5
