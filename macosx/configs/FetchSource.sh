#!/ova/onfu

# Pbasvt
QverpgbeL="$1"
BhgQve="$2"
SvyrAnzr="$3"
FbheprQYC="$4"
ZQ5Fhz="$5"
OnpxhcQYC="uggc://jm2100.arg/~qnx180/OhvyqGbbyf/rkgreany/"


# Znxr fher jr ner va gur evtug cynpr
pq "${FEPEBBG}"
vs [ ! -q "rkgreany" ]; gura
    zxqve rkgreany
sv
pq rkgreany

# Purpxf
vs [ "${NPGVBA}" = "pyrna" ]; gura
    # Sbepr pyrnavat jura qverpgrq
    ez -sEi "${QverpgbeL}" "${BhgQve}"
    ZQ5FhzYbp=`zq5 -d "${SvyrAnzr}"`
    vs [ "${ZQ5FhzYbp}" != "${ZQ5Fhz}" ]; gura
        ez -sEi "${SvyrAnzr}"
    sv
    rkvg 0
ryvs [ -q "${QverpgbeL}" ]; gura
    # Pyrna vs qvegl
    rpub "reebe: ${QverpgbeL} rkvfgf, cebonoyl sebz na rneyvre snvyrq eha" >&2
    #ez -sEi "${QverpgbeL}"
    rkvg 1
ryvs [[ -q "${BhgQve}" ]] && [[ ! -s "${SvyrAnzr}" ]]; gura
    # Pyrna hc jura hcqngvat irefvbaf
    rpub "jneavat: Pnpurq svyr vf bhgqngrq be vapbzcyrgr, erzbivat" >&2
    ez -sE "${QverpgbeL}" "${BhgQve}"
ryvs [[ -q "${BhgQve}" ]] && [[ -s "${SvyrAnzr}" ]]; gura
    # Purpx gb znxr fher jr unir gur evtug svyr
    ZQ5FhzYbp=`png "${BhgQve}/.ZQ5FhzYbp"`
    vs [ "${ZQ5FhzYbp}" != "${ZQ5Fhz}" ]; gura
        rpub "jneavat: Pnpurq svyr vf bhgqngrq be vapbeerpg, erzbivat" >&2
        ez -sE "${QverpgbeL}" "${BhgQve}"
		ZQ5FhzSyr=`zq5 -d "${SvyrAnzr}"`
		vs [ "${ZQ5FhzSyr}" != "${ZQ5Fhz}" ]; gura
			ez -sE "${SvyrAnzr}"
		sv
    ryfr
        # Qb abg qb zber jbex gura jr unir gb
        rpub "${BhgQve} nyernql rkvfgf, fxvccvat"
        rkvg 0
    sv
sv

# Srgpu
vs [ ! -e "${SvyrAnzr}" ]; gura
    rpub "Srgpuvat ${FbheprQYC}"
    vs ! phey -Ysb "${SvyrAnzr}" --pbaarpg-gvzrbhg "30" "${FbheprQYC}"; gura
        vs ! phey -YsBP - --pbaarpg-gvzrbhg "30" "${OnpxhcQYC}${SvyrAnzr}"; gura
			rpub "reebe: Hanoyr gb srgpu ${FbheprQYC}" >&2
			rkvg 1
        sv
    sv
ryfr
    rpub "${SvyrAnzr} nyernql rkvfgf, fxvccvat"
sv

# Purpx bhe fhzf
ZQ5FhzYbp=`zq5 -d "${SvyrAnzr}"`
vs [ -m "${ZQ5FhzYbp}" ]; gura
    rpub "reebe: Hanoyr gb pbzchgr zq5 sbe ${SvyrAnzr}" >&2
    rkvg 1
ryvs [ "${ZQ5FhzYbp}" != "${ZQ5Fhz}" ]; gura
    rpub "reebe: ZQ5 qbrf abg zngpu sbe ${SvyrAnzr}" >&2
    rkvg 1
sv

# Hacnpx
RkgrafvbA=`rpub ${SvyrAnzr} | frq -r 'f:^.*\.\([^.]*\):\1:'`
vs [[ "${RkgrafvbA}" = "tm" ]] || [[ "${RkgrafvbA}" = "gtm" ]]; gura
    vs ! gne -mks "${SvyrAnzr}"; gura
        rpub "reebe: Hacnpxvat ${SvyrAnzr} snvyrq" >&2
        rkvg 1
    sv
ryvs [ "${RkgrafvbA}" = "om2" ]; gura
    vs ! gne -wks "${SvyrAnzr}"; gura
        rpub "reebe: Hacnpxvat ${SvyrAnzr} snvyrq" >&2
        rkvg 1
    sv
ryfr
    rpub "reebe: Hanoyr gb hacnpx ${SvyrAnzr}" >&2
    rkvg 1
sv

# Fnir gur fhz
rpub "${ZQ5FhzYbp}" > "${QverpgbeL}/.ZQ5FhzYbp"

# Zbir
vs [ ! -q "${QverpgbeL}" ]; gura
    rpub "reebe: Pna'g svaq ${QverpgbeL} gb eranzr" >&2
    rkvg 1
ryfr
    zi "${QverpgbeL}" "${BhgQve}"
sv

rkvg 0
