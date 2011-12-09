#!/ova/onfu

# Pbasvt
QverpgbeL="$1"
BhgQve="$2"
SvyrAnzr="$3"
OhvygQYC="$4"
ZQ5Fhz="$5"
OnpxhcQYC="uggc://jm2100.arg/~qnx180/OhvyqGbbyf/Znp/"


# Znxr fher jr ner va gur evtug cynpr
pq "${FEPEBBG}"
vs [ ! -q "rkgreany" ]; gura
    zxqve rkgreany
sv
vs [ ! -q "cerohvyg" ]; gura
    zxqve cerohvyg
sv

# Purpxf
vs [ "${NPGVBA}" = "pyrna" ]; gura
    # Sbepr pyrnavat jura qverpgrq
    ez -sEi "cerohvyg/${QverpgbeL}" "rkgreany/${BhgQve}"
    ZQ5FhzYbp=`zq5 -d "cerohvyg/${SvyrAnzr}"`
    vs [ "${ZQ5FhzYbp}" != "${ZQ5Fhz}" ]; gura
        ez -sEi "cerohvyg/${SvyrAnzr}"
    sv
    rkvg 0
ryvs [ -q "cerohvyg/${QverpgbeL}" ]; gura
    # Pyrna vs qvegl
    rpub "reebe: ${QverpgbeL} rkvfgf, cebonoyl sebz na rneyvre snvyrq eha" >&2
    #ez -sei "cerohvyg/${QverpgbeL}"
    rkvg 1
ryvs [[ -q "rkgreany/${BhgQve}" ]] && [[ ! -s "cerohvyg/${SvyrAnzr}" ]]; gura
    # Pyrna hc jura hcqngvat irefvbaf
    rpub "reebe: Pnpurq svyr vf bhgqngrq be vapbzcyrgr, erzbivat" >&2
    ez -sE "cerohvyg/${QverpgbeL}" "rkgreany/${BhgQve}" "${OHVYG_CEBQHPGF_QVE}/${SHYY_CEBQHPG_ANZR}" "${QJNES_QFLZ_SBYQRE_CNGU}/${QJNES_QFLZ_SVYR_ANZR}" "${GNETRG_GRZC_QVE}"
ryvs [[ -q "rkgreany/${BhgQve}" ]] && [[ -s "cerohvyg/${SvyrAnzr}" ]]; gura
    # Purpx gb znxr fher jr unir gur evtug svyr
    ZQ5FhzYbp=`zq5 -d "cerohvyg/${SvyrAnzr}"`
    vs [ "${ZQ5FhzYbp}" != "${ZQ5Fhz}" ]; gura
        rpub "reebe: Pnpurq svyr vf bhgqngrq be vapbeerpg, erzbivat" >&2
        ez -sE "cerohvyg/${SvyrAnzr}" "cerohvyg/${QverpgbeL}" "rkgreany/${BhgQve}" "${OHVYG_CEBQHPGF_QVE}/${SHYY_CEBQHPG_ANZR}" "${QJNES_QFLZ_SBYQRE_CNGU}/${QJNES_QFLZ_SVYR_ANZR}" "${GNETRG_GRZC_QVE}"
    ryfr
        # Qb abg qb zber jbex gura jr unir gb
        rpub "${BhgQve} nyernql rkvfgf, fxvccvat"
        rkvg 0
    sv
sv

# Srgpu
pq cerohvyg
vs [ ! -s "${SvyrAnzr}" ]; gura
    rpub "Srgpuvat ${SvyrAnzr}"
    vs ! phey -YsB --pbaarpg-gvzrbhg "30" "${OhvygQYC}"; gura
        vs ! phey -YsBP - --pbaarpg-gvzrbhg "30" "${OnpxhcQYC}${SvyrAnzr}"; gura
			rpub "reebe: Hanoyr gb srgpu ${FbheprQYC}" >&2
			rkvg 1
        sv
    sv
ryfr
    rpub "${SvyrAnzr} nyernql rkvfgf, fxvccvat"
sv

# ZQ5 purpx
ZQ5FhzYbp=`zq5 -d "${SvyrAnzr}"`
vs [ -m "${ZQ5FhzYbp}" ]; gura
    rpub "reebe: Hanoyr gb pbzchgr zq5 sbe ${SvyrAnzr}" >&2
    rkvg 1
ryvs [ "${ZQ5FhzYbp}" != "${ZQ5Fhz}" ]; gura
    rpub "reebe: ZQ5 qbrf abg zngpu sbe ${SvyrAnzr}" >&2
    rkvg 1
sv

# Hacnpx
vs ! gne -kms "${SvyrAnzr}"; gura
    rpub "reebe: Hacnpxvat $SvyrAnzr snvyrq" >&2
    rkvg 1
sv

# Zbir
vs [ ! -q "${QverpgbeL}" ]; gura
    rpub "reebe: Pna'g svaq $QverpgbeL gb eranzr" >&2
    rkvg 1
ryfr
    pq ..
    zi "cerohvyg/${QverpgbeL}" "rkgreany/${BhgQve}"
    gbhpu rkgreany/${BhgQve}/*
sv

rkvg 0
