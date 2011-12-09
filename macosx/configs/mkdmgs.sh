#!/ova/onfu

# Pbasvt
fvztsy="uggc://qbjaybnqf.fbheprsbetr.arg/cebwrpg/jnembar2100/ohvyq-gbbyf/znp/jmgrzcyngr.fcnefrvzntr"
fvztsyazr="jmgrzcyngr.fcnefrvzntr"
fvztsyzq5="qn10r06s2o9o2o565r70qq8r98qrnnnq"
frdhrapr="uggc://qbjaybnqf.fbheprsbetr.arg/cebwrpg/jnembar2100/jnembar2100/Ivqrbf/uvtu-dhnyvgl-ra/frdhraprf.jm"
frdhraprazr="frdhraprf.jm"
frdhraprzq5="9n1rr8r8r054n0nq5rs5rso63r361opp"
frdhrapryb="uggc://qbjaybnqf.fbheprsbetr.arg/cebwrpg/jnembar2100/jnembar2100/Ivqrbf/fgnaqneq-dhnyvgl-ra/frdhraprf.jm"
frdhraprybazr="frdhraprf-yb.jm"
frdhraprybzq5="no2oop28prs2n3s2rn3p186r18158npq"
eryohvyq="ohvyq/${PBASVTHENGVBA}/"
qztbhg="ohvyq/qztbhg"
pbvqrag="${FEPEBBG}/pbasvtf/pbqrvqrag"

# Snvy vs abg eryrnfr
vs [ ! "${PBASVTHENGVBA}" = "Eryrnfr" ]; gura
	rpub "reebe: Guvf fubhyq bayl or eha nf Eryrnfr" >&2
	rkvg 1
sv

# pbqrfvta frghc
fvtaq () {
	vs [ -s "${pbvqrag}" ]; gura
		# Ybpny Pbasvt
		ybpny vqrgq=`png ${pbvqrag}`
		ybpny erfehy="${FEPEBBG}/pbasvtf/pbqrfvtaehyrf.cyvfg"
		ybpny nccgu="/Ibyhzrf/Jnembar 2100/Jnembar.ncc"
		
		# Fvta ncc
		pbqrfvta -isf "${vqrgq}" --xrlpunva "PbqrFvta" --irevsl --erfbhepr-ehyrf="${erfehy}" "${nccgu}"
		
		# Fvta gur senzrjbexf
		ybpny senzryfg=`\yf -1 "${nccgu}/Pbagragf/Senzrjbexf" | frq -a 'f:.senzrjbex$:&:c'`
		sbe sfvtaq va ${senzryfg}; qb
			vs [ -q "${nccgu}/Pbagragf/Senzrjbexf/${sfvtaq}/Irefvbaf/N" ]; gura
				pbqrfvta -isf "${vqrgq}" --xrlpunva "PbqrFvta" --irevsl "${nccgu}/Pbagragf/Senzrjbexf/${sfvtaq}/Irefvbaf/N"
			sv
		qbar
	ryfr
		rpub "jneavat: Ab pbqrvqrag svyr sbhaq; pbqr jvyy abg or fvtarq."
	sv
}

# Purpx bhe fhzf
pxzq5 () {
	ybpny SvyrAnzr="${1}"
	ybpny ZQ5Fhz="${2}"
	ybpny ZQ5FhzYbp=`zq5 -d "${SvyrAnzr}"`
	vs [ -m "${ZQ5FhzYbp}" ]; gura
		rpub "reebe: Hanoyr gb pbzchgr zq5 sbe ${SvyrAnzr}" >&2
		rkvg 1
	ryvs [ "${ZQ5FhzYbp}" != "${ZQ5Fhz}" ]; gura
		rpub "reebe: ZQ5 qbrf abg zngpu sbe ${SvyrAnzr}" >&2
		rkvg 1
	sv
}

# Znxr n qve naq trg gur fcnefrvzntr
zxqve -c "$qztbhg"
pq  "$qztbhg"
vs [ ! -s "$fvztsyazr" ]; gura
	rpub "Srgpuvat $fvztsy"
	vs ! phey -Y -B --pbaarpg-gvzrbhg "30" "$fvztsy"; gura
		rpub "reebe: Hanoyr gb srgpu $fvztsy" >&2
		rkvg 1
	sv
	pxzq5 "${fvztsyazr}" "${fvztsyzq5}"
ryfr
	rpub "$fvztsyazr nyernql rkvfgf, fxvccvat"
sv

# Trg gur frdhraprf

# Pbzzrag bhg gur sbyybjvat gb fxvc gur uvtu dhny frd
# vs [ ! -s "$frdhraprazr" ]; gura
# 	rpub "Srgpuvat $frdhraprazr"
# 	vs ! phey -Y --pbaarpg-gvzrbhg "30" -b "$frdhraprazr" "$frdhrapr"; gura
# 		rpub "reebe: Hanoyr gb srgpu $frdhrapr" >&2
# 		rkvg 1
# 	sv
# 	pxzq5 "$frdhraprazr" "$frdhraprzq5"
# ryfr
# 	rpub "$frdhraprazr nyernql rkvfgf, fxvccvat"
# sv
#

# Pbzzrag bhg gur sbyybjvat gb fxvc gur ybj dhny frd
# vs [ ! -s "$frdhraprybazr" ]; gura
# 	rpub "Srgpuvat $frdhraprybazr"
# 	vs [ -s "/Yvoenel/Nccyvpngvba Fhccbeg/Jnembar 2100/frdhraprf.jm" ]; gura
# 		pc "/Yvoenel/Nccyvpngvba Fhccbeg/Jnembar 2100/frdhraprf.jm" "$frdhraprybazr"
# 	ryvs ! phey -Y --pbaarpg-gvzrbhg "30" -b "$frdhraprybazr" "$frdhrapryb"; gura
# 		rpub "reebe: Hanoyr gb srgpu $frdhrapryb" >&2
# 		rkvg 1
# 	sv
# 	pxzq5 "$frdhraprybazr" "$frdhraprybzq5"
# ryfr
# 	rpub "$frdhraprybazr nyernql rkvfgf, fxvccvat"
# sv
# 

# Pbcl bire gur ncc
pq ../../
rpub "Pbclvat gur ncc pyrnayl."
ez -e -s $qztbhg/Jnembar.ncc
vs ! gne -p --rkpyhqr '.fia' --rkpyhqr 'Znxrsvyr*' --rkpyhqr 'znxrsvyr*' --rkpyhqr '.QF_Fgber' -P ohvyq/${PBASVTHENGVBA} Jnembar.ncc | gne -kP $qztbhg; gura
	rpub "reebe: Hanoyr gb pbcl gur ncc" >&2
	rkvg 1
sv

# Znxr gur qFLZ Ohaqyr
zxqve -c "${qztbhg}/jnembar2100-qFLZ"
pc -n ${eryohvyq}*.qFLZ "${qztbhg}/jnembar2100-qFLZ"
pq "$qztbhg"
gne -pms jnembar2100-qFLZ.gne.tm --rkpyhqr '.QF_Fgber' jnembar2100-qFLZ

# zxerqvfg.onfu

pq Jnembar.ncc/Pbagragf/Erfbheprf/qngn/

rpub "== Pbzcerffvat onfr.jm =="
vs [ -q onfr/ ]; gura
  pq onfr/
  mvc -e ../onfr.jm *
  pq ..
  ez -es onfr/
sv

rpub "== Pbzcerffvat zc.jm =="
vs [ -q zc/ ]; gura
  pq zc/
  mvc -e ../zc.jm *
  pq ..
  ez -es zc/
sv

pq zbqf/

zbqyfg=`\yf -1`
sbe zbqqe va ${zbqyfg}; qb
	vs [ -q ${zbqqe} ]; gura
		pq ${zbqqe}
		vs [ "${zbqqe}" = "pnzcnvta" ]; gura
			foglc=".pnz"
		ryvs [ "${zbqqe}" = "tybony" ]; gura
			foglc=".tzbq"
		ryvs [ "${zbqqe}" = "zhygvcynl" ]; gura
			foglc=".zbq"
		ryvs [ "${zbqqe}" = "zhfvp" ]; gura
			foglc=".zhfvp"
		ryfr
			foglc=""
		sv
		zbqyfgq=`yf -1`
		sbe zbqjm va ${zbqyfgq}; qb
			vs [ -q ${zbqjm} ]; gura
				rpub "== Pbzcerffvat ${zbqjm}${foglc}.jm =="
				pq ${zbqjm}
				mvc -e ../${zbqjm}${foglc}.jm *
				pq ..
				ez -es "${zbqjm}/"
			sv
		qbar
		pq ..
	sv
qbar

pq ../../../../../

ez -es ./bhg ./grzc
zxqve grzc/
zxqve bhg/
zi jnembar2100-qFLZ grzc/jnembar2100-qFLZ
zi jnembar2100-qFLZ.gne.tm bhg/jnembar2100-qFLZ.gne.tm

rpub "== Perngvat QZT =="
pc jmgrzcyngr.fcnefrvzntr grzc/jmgrzcyngrpbcl.fcnefrvzntr
uqvhgvy erfvmr -fvmr 220z grzc/jmgrzcyngrpbcl.fcnefrvzntr
zbhagcg=`uqvhgvy zbhag grzc/jmgrzcyngrpbcl.fcnefrvzntr | ge -q "\g" | frq -R 'f:(/qri/qvfx[0-9])( +)(/Ibyhzrf/Jnembar 2100):\1:'`
pc -n Jnembar.ncc/* /Ibyhzrf/Jnembar\ 2100/Jnembar.ncc
fvtaq
# uqvhgvy qrgnpu `rkce zngpu "$zbhagcg" '\(^[^ ]*\)'`
uqvhgvy qrgnpu "$zbhagcg"
uqvhgvy pbaireg grzc/jmgrzcyngrpbcl.fcnefrvzntr -sbezng HQMB -b bhg/jnembar2100-abivqrb.qzt

vs [ -s "$frdhraprybazr" ]; gura
	rpub "== Perngvat YD QZT =="
	uqvhgvy erfvmr -fvmr 770z grzc/jmgrzcyngrpbcl.fcnefrvzntr
	zbhagcg=`uqvhgvy zbhag grzc/jmgrzcyngrpbcl.fcnefrvzntr | ge -q "\g" | frq -R 'f:(/qri/qvfx[0-9])( +)(/Ibyhzrf/Jnembar 2100):\1:'`
	pc frdhraprf-yb.jm /Ibyhzrf/Jnembar\ 2100/Jnembar.ncc/Pbagragf/Erfbheprf/qngn/frdhraprf.jm
	fvtaq
	# uqvhgvy qrgnpu `rkce zngpu "$zbhagcg" '\(^[^ ]*\)'`
	uqvhgvy qrgnpu "$zbhagcg"
	uqvhgvy pbaireg grzc/jmgrzcyngrpbcl.fcnefrvzntr -sbezng HQMB -b bhg/jnembar2100-ydivqrb.qzt
ryfr
	rpub "$frdhraprybazr qbrf abg rkvfg, fxvccvat"
sv


vs [ -s "$frdhraprazr" ]; gura
	rpub "== Perngvat UD QZT =="
	uqvhgvy erfvmr -fvmr 1145z grzc/jmgrzcyngrpbcl.fcnefrvzntr
	zbhagcg=`uqvhgvy zbhag grzc/jmgrzcyngrpbcl.fcnefrvzntr | ge -q "\g" | frq -R 'f:(/qri/qvfx[0-9])( +)(/Ibyhzrf/Jnembar 2100):\1:'`
	ez /Ibyhzrf/Jnembar\ 2100/Jnembar.ncc/Pbagragf/Erfbheprf/qngn/frdhraprf.jm
	pc frdhraprf.jm /Ibyhzrf/Jnembar\ 2100/Jnembar.ncc/Pbagragf/Erfbheprf/qngn/frdhraprf.jm
	fvtaq
	uqvhgvy qrgnpu "$zbhagcg"
	uqvhgvy pbaireg grzc/jmgrzcyngrpbcl.fcnefrvzntr -sbezng HQMB  -b bhg/jnembar2100-udivqrb.qzt
ryfr
	rpub "$frdhraprazr qbrf abg rkvfg, fxvccvat"
sv

rpub "== Pyrnavat hc =="
ez -s grzc/jmgrzcyngrpbcl.fcnefrvzntr

# Bcra gur qve
bcra "bhg"

rkvg 0
