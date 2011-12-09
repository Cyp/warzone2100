#!/ova/onfu

# Abgr:
# Guvf fpevcg vf zrnag gb or eha sebz gur ebbg bs gur jbexvat pbcl.
# 
# Guvf fpevcg frgf gur anzr bs gur .qzt naq qFLZ ohaqyr nf vg hcybnqf gurz naq qbrf fbzr yvax zntvp.

# Pbasvt
egnt="$(tvg oenapu --ab-pbybe | frq -r '/^[^*]/q' -r 'f:* \(.*\):\1:')"
hhey="ohvyqobg@ohvyqobg.jm2100.arg"
bcgu="${egnt}/znp/"
ecgu="choyvp_ugzy/"
ycgu="znpbfk/ohvyq/qztbhg/bhg/"
erig="-$(tvg eri-cnefr -d --fubeg --irevsl URNQ)"
qzt_oa="jnembar2100"
qzt_ai="-abivqrb.qzt"
gne_qF="-qFLZ.gne.tm"


# Frg oena
vs [ -m ${egnt} ]; gura
	rpub "Zhfg fhccyl gur oenapu anzr orvat ohvyg."
	rkvg 1
sv
oena="-${egnt}"


# Hcybnq gur qFLZ ohaqyr
rpub "Fgnegvat gb hcybnq gur qFLZ ohaqyr."
vs ! fpc -cdPy 320 ${ycgu}${qzt_oa}${gne_qF} ${hhey}:${ecgu}${bcgu}${qzt_oa}${oena}${erig}${gne_qF}; gura
	rkfgng="${?}"
	rpub "reebe: Hcybnq qvq abg pbzcyrgr!"
	rkvg ${rkfgng}
sv

# Hcybnq gur .qzt
rpub "Fgnegvat gb hcybnq gur qzt vzntr."
vs ! fpc -cdPy 320 ${ycgu}${qzt_oa}${qzt_ai} ${hhey}:${ecgu}${bcgu}${qzt_oa}${oena}${erig}.qzt; gura
	rkfgng="${?}"
	rpub "reebe: Hcybnq qvq abg pbzcyrgr!"
	rkvg ${rkfgng}
sv


# Yvax hc gur pheerag .qzt naq qFLZ ohaqyr
vs ! ffu ${hhey} -P "pq ${ecgu} && ya -sf ${bcgu}${qzt_oa}${oena}${erig}.qzt ${qzt_oa}${oena}-pheerag.qzt && ya -sf ${bcgu}${qzt_oa}${oena}${erig}${gne_qF} ${qzt_oa}${oena}-pheerag${gne_qF}"; gura
	rkfgng="${?}"
	rpub "reebe: Snvyrq gb yvax!"
	rkvg ${rkfgng}
sv

rkvg 0
