# Licencias y aspectos legales

## Resumen ejecutivo

AINE es GPL v3. Puedes usarlo, modificarlo y distribuirlo libremente. No puedes venderlo como software cerrado ni distribuirlo en la Mac App Store sin publicar todo el código fuente.

---

## Componentes y sus licencias

| Componente | Licencia | Implicación para AINE |
|---|---|---|
| ATL (base) | GPL v3 | AINE hereda GPL v3 |
| AOSP (ART, Framework) | Apache 2.0 | Compatible con GPL v3 |
| ANGLE (Google) | BSD 3-Clause | Compatible con GPL v3 |
| MoltenVK (Khronos) | Apache 2.0 | Compatible con GPL v3 |
| Darling (referencia) | GPL v2/v3 | Solo referencia — no copiar código |
| Código nuevo de AINE | GPL v3 | Consistente con el resto |

---

## GPL v3: qué significa en la práctica

### Sí puedes hacer

- Distribuir AINE como software libre con código fuente completo
- Cobrar por distribución física o servicios de instalación
- Ofrecer soporte comercial, consultoría, desarrollo a medida
- Solicitar donaciones y sponsorships (GitHub Sponsors, Open Collective)
- Solicitar financiación pública (NLnet, NGI Zero, STF)
- Operar un "store" de apps AOSP/FOSS donde las apps en sí no son parte de AINE
- Hacer fork con tu propia marca si mantienes GPL v3

### No puedes hacer

- Distribuir AINE como binario cerrado sin ofrecer el código fuente
- Incluir AINE como componente de un producto propietario
- Distribuir en la Mac App Store (las condiciones de Apple son incompatibles con GPL v3)
- Cambiar la licencia a algo más restrictivo en distribuciones derivadas

---

## El problema de la Mac App Store con GPL

La GPL v3 requiere que cualquier usuario que reciba el software pueda también recibir el código fuente completo y tener libertad de modificarlo. Las condiciones de la Mac App Store incluyen restricciones de uso (DRM, restricciones de redistribución) que son incompatibles con esta libertad.

Este es un problema conocido y documentado — VLC fue retirado de la App Store en 2010 por exactamente esta razón.

**Opciones si quieres estar en la Mac App Store:**
1. Dual-licensing: ofrecer AINE bajo GPL v3 (para la comunidad) Y bajo una licencia comercial (para la App Store). Requiere que seas el propietario del 100% del copyright, o que todos los contribuidores firmen un CLA.
2. Reescribir las partes de ATL desde cero bajo tu propia licencia (estimación: +3-4 meses de trabajo).
3. Distribuir via Homebrew, DMG directo, o Sparkle — perfectamente válido con GPL v3.

**Recomendación:** Distribución via Homebrew + DMG para v1. La App Store puede ser objetivo de v2 si tiene tracción suficiente para justificar el trabajo de dual-licensing.

---

## Acerca de Google y Android

Google no puede demandar por usar AOSP — es exactamente para lo que sirve la licencia Apache 2.0. La condición es no usar el nombre "Android" como marca registrada para el producto final (de ahí que AINE diga "compatible con apps Android AOSP" y no "Android para macOS").

El nombre "Android" es una marca registrada de Google. AINE debe evitar en su marketing:
- "Android en macOS" como nombre del producto
- El logo de Android (el robot verde)
- Cualquier implicación de afiliación con Google

Términos seguros: "apps AOSP", "apps Android open source", "compatibilidad con apps Android FOSS".

---

## CLA (Contributor License Agreement)

Si en el futuro quieres hacer dual-licensing, necesitas que todos los contribuidores firmen un CLA. Recomendación: implementarlo desde el primer PR externo usando CLA Assistant (gratuito, integración con GitHub).

```
# .github/workflows/cla.yml — activar cuando el proyecto tenga contribuidores externos
```

---

## Financiación compatible con GPL v3

Estas fuentes de financiación son perfectamente compatibles con AINE como proyecto GPL:

- **NLnet Foundation / NGI Zero**: ATL recibió financiación de aquí. Convocatorias abiertas regularmente. https://nlnet.nl/propose/
- **STF (Sovereign Tech Fund)**: Fondo alemán para infraestructura digital. https://www.sovereigntechfund.de/
- **GitHub Sponsors**: Donaciones directas. Activar en el perfil cuando haya una demo.
- **Open Collective**: Para transparencia de gastos de proyecto. Útil si hay equipo.
