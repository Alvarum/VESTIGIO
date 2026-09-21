# Oleadas y dependencias

Generado de [backlog.json](backlog.json). Una oleada expresa profundidad de dependencias, no una barrera obligatoria ni autorización para escribir simultáneamente. Se puede adelantar un ticket cuando sus dependencias estén INTEGRATED y sus locks/archivos estén libres.

El coordinador puede usar menos workers que tickets. Además de locks se revisa el solapamiento real de archivos; ver [PLAN.md](PLAN.md).

| Oleada teórica | Tickets | Conflictos de locks dentro de la oleada |
|---|---|---|
| W00 | F00 | Ninguno declarado; verificar archivos |
| W01 | F01 | Ninguno declarado; verificar archivos |
| W02 | G01, R01 | Ninguno declarado; verificar archivos |
| W03 | G02, R02, D01 | Ninguno declarado; verificar archivos |
| W04 | R03, M01, D02 | Ninguno declarado; verificar archivos |
| W05 | I01, G03 | I01/G03: public-api |
| W06 | G04, E01, S01, V01, A01, A02 | G04/V01: gpu-backend; G04/A02: gpu-backend; S01/A01: public-api; V01/A02: gpu-backend |
| W07 | D03, E02, S02, V02 | Ninguno declarado; verificar archivos |
| W08 | E03, S03 | Ninguno declarado; verificar archivos |
| W09 | E04, P01 | Ninguno declarado; verificar archivos |
| W10 | E05 | Ninguno declarado; verificar archivos |
| W11 | Q01 | Ninguno declarado; verificar archivos |
| W12 | P02 | Ninguno declarado; verificar archivos |
| W13 | T01 | Ninguno declarado; verificar archivos |
| W14 | Z01 | Ninguno declarado; verificar archivos |

## DAG completo

```mermaid
flowchart TD
  F00[F00]
  F01[F01]
  G01[G01]
  R01[R01]
  G02[G02]
  R02[R02]
  R03[R03]
  I01[I01]
  D01[D01]
  M01[M01]
  G03[G03]
  G04[G04]
  D02[D02]
  D03[D03]
  E01[E01]
  E02[E02]
  E03[E03]
  E04[E04]
  S01[S01]
  S02[S02]
  S03[S03]
  V01[V01]
  V02[V02]
  A01[A01]
  A02[A02]
  E05[E05]
  Q01[Q01]
  P01[P01]
  P02[P02]
  T01[T01]
  Z01[Z01]
  F00 --> F01
  F01 --> G01
  F01 --> R01
  G01 --> G02
  R01 --> R02
  R02 --> R03
  R03 --> I01
  R01 --> D01
  R02 --> M01
  G01 --> G03
  R03 --> G03
  M01 --> G03
  G03 --> G04
  D01 --> D02
  R02 --> D02
  D02 --> D03
  G04 --> D03
  G02 --> E01
  G03 --> E01
  D02 --> E01
  I01 --> E01
  E01 --> E02
  E02 --> E03
  M01 --> E03
  E03 --> E04
  D03 --> E04
  S01 --> E04
  G03 --> S01
  D02 --> S01
  S01 --> S02
  I01 --> S02
  S02 --> S03
  D02 --> S03
  R03 --> S03
  G03 --> V01
  I01 --> V01
  D02 --> V01
  V01 --> V02
  R02 --> A01
  I01 --> A01
  D02 --> A01
  G03 --> A02
  D02 --> A02
  E04 --> E05
  S03 --> E05
  V02 --> E05
  A01 --> E05
  A02 --> E05
  D03 --> E05
  E05 --> Q01
  S03 --> P01
  A01 --> P01
  A02 --> P01
  R03 --> P01
  D02 --> P01
  E05 --> P02
  P01 --> P02
  Q01 --> P02
  P02 --> T01
  P02 --> Z01
  T01 --> Z01
```

## Elegibilidad actual

Por estado de dependencias: A01.

Filtrar después por alcance encargado y locks. BLOCKED requiere resolver su motivo y actualizar estado; no se relanza automáticamente.
