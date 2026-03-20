# Guía de desarrollo con agentes AI

Esta guía define cómo usar agentes AI (Claude, Cursor, Copilot, Aider, etc.) de forma efectiva en el desarrollo de AINE. El proyecto tiene partes con complejidad muy diferente — algunas son perfectas para delegación a agentes, otras requieren juicio humano experto.

---

## Principios generales

### Qué funciona bien con agentes
- Generación de código boilerplate (stubs, tests, CMakeLists)
- Traducción de patrones conocidos (epoll→kqueue ya está documentado)
- Documentación y comentarios de código
- Scripts de build y tooling
- Adaptación de código C/C++ a diferente plataforma cuando el patrón es claro
- Code review y detección de bugs

### Qué requiere juicio humano
- Decisiones de arquitectura que afectan múltiples componentes
- Debugging de crashes en el límite user/kernel space
- Performance profiling y optimización a bajo nivel
- Decisiones sobre qué partes de ATL adoptar vs reimplementar

---

## Contexto base para cualquier sesión AI

Copia y pega esto al inicio de cualquier sesión sobre AINE:

```
Contexto del proyecto AINE:

AINE es una capa de compatibilidad (estilo Wine) para ejecutar apps Android AOSP 
en macOS de forma nativa, sin emulación de CPU. Apple Silicon es ARM64 — misma 
arquitectura que Android. AINE traduce APIs Android → macOS.

Base de código: fork de ATL (Android Translation Layer, GPL v3) que hace lo mismo 
en Linux. AINE añade la capa específica de macOS: syscall shim Linux→XNU, 
ANGLE para OpenGL ES→Metal, Binder sobre Mach IPC, HAL bridges para 
CoreAudio/AVFoundation/NSEvent.

Stack: C/C++ para el shim y runtime, Objective-C para los HAL bridges, 
SwiftUI para el launcher. Build: CMake + Ninja. Target: macOS 13+ ARM64.

Repositorio ATL de referencia: https://gitlab.com/android_translation_layer/android_translation_layer
Referencia para syscalls macOS: https://github.com/darlinghq/darling
```

---

## Prompts por área de trabajo

### Syscall shim (aine-shim)

**Para implementar una nueva traducción de syscall:**
```
Necesito implementar [SYSCALL_NAME] de Linux en macOS para el proyecto AINE.

La syscall Linux hace: [descripción del comportamiento]
La semántica exacta que Android necesita es: [casos de uso específicos]

En macOS los equivalentes son: [kqueue / dispatch / etc.]

Por favor:
1. Implementa la función C con DYLD_INTERPOSE
2. Maneja los casos edge: [lista]
3. Añade tests en tests/shim/test_[nombre].c
4. Usa el estilo del proyecto: C11, sin excepciones, error via errno

Referencia de cómo lo hace Darling: [pegar código de Darling si existe]
```

**Para depurar un crash en el shim:**
```
AINE crashea con el siguiente stack trace al intentar [acción]:

[pegar stack trace]

El crash ocurre en [componente]. Sospecho que es [hipótesis].

Por favor analiza el stack trace, identifica la causa raíz y propón
un fix. Ten en cuenta que estamos en macOS ARM64 con aine-shim inyectado
via DYLD_INSERT_LIBRARIES.
```

---

### Binder IPC (aine-binder)

**Para implementar un servicio nuevo:**
```
Necesito portar el servicio Android [NombreServicio] para aine-binder.

El servicio en AOSP está en: [ruta en AOSP]
Su interfaz AIDL es: [pegar interfaz]

Para AINE v1, el servicio debe:
- [funcionalidad mínima requerida]
- Stubs para: [métodos que no necesitamos implementar aún]

Por favor genera:
1. La implementación del servicio en C++ usando aine-binder
2. El stub de registro en system_server
3. Un test de integración básico
```

---

### HAL bridges (aine-hals)

**Para un nuevo HAL:**
```
Necesito implementar el HAL de [componente] para AINE.

La interfaz Android HAL es: [ruta en AOSP, e.g. hardware/interfaces/audio/]
El framework macOS equivalente es: [CoreAudio / AVFoundation / etc.]

Flujo de datos:
Android app → [ruta] → HAL de AINE → [framework macOS]

Por favor implementa en Objective-C++:
1. La estructura hardware_module_t requerida por Android
2. La traducción de [formato Android] → [formato macOS]
3. El manejo del ciclo de vida (open/close del HAL)
```

---

### Build system y tooling

**Para añadir un nuevo módulo al build:**
```
Necesito añadir un nuevo módulo CMake a AINE para [nombre-modulo].

El módulo:
- Es una dylib / executable / static lib
- Depende de: [dependencias]
- Sus fuentes están en src/[nombre]/
- En Linux se llamaría [nombre en ATL]
- Necesita los headers stub de aine-shim para: [headers]

Genera el CMakeLists.txt para este módulo siguiendo el estilo 
del resto del proyecto (ver src/aine-shim/CMakeLists.txt como 
referencia).
```

---

### Sincronización con ATL upstream

**Para evaluar un commit de ATL:**
```
El proyecto ATL (upstream de AINE) tiene el siguiente commit que quiero evaluar:

Commit: [hash]
Mensaje: [mensaje del commit]
Diff: [pegar diff]

Por favor:
1. Determina si este cambio es relevante para AINE
2. Si es relevante, ¿se puede aplicar directamente o necesita adaptación para macOS?
3. Si necesita adaptación, identifica qué partes son Linux-específicas y cómo adaptarlas
4. ¿Hay algún riesgo de regresión al aplicarlo?
```

---

## Flujo de trabajo con Cursor/Aider

### Estructura de sesiones recomendada

Para tareas de implementación de más de 2 horas:

```
1. CONTEXTO (5 min)
   - Pega el contexto base de AINE
   - Indica el milestone actual (ver ROADMAP.md)
   - Muestra el estado actual del componente a trabajar

2. OBJETIVO (2 min)
   - Define el criterio de éxito concreto (igual que en ROADMAP.md)
   - Indica qué tests deben pasar al final

3. RESTRICCIONES (2 min)
   - "No modifiques src/aine-binder/ en esta sesión"
   - "El código debe compilar en macOS ARM64 clang"
   - "Mantén compatibilidad con ATL upstream en [componente]"

4. IMPLEMENTACIÓN
   - El agente genera código
   - Tú compilas y testeas en el Mac real
   - Iterar

5. CIERRE (5 min)
   - Pide al agente que documente los cambios
   - Pide que añada tests si faltan
   - Commit con mensaje descriptivo
```

### Comandos Aider útiles para AINE

```bash
# Sesión de implementación de syscall
aider src/aine-shim/epoll.c src/aine-shim/include/sys/epoll.h \
      tests/shim/test_epoll.c \
      --message "Implementa epoll sobre kqueue siguiendo el contexto en docs/ai-agents/AGENTS.md"

# Revisión de diff de ATL
aider --read vendor/atl/[fichero_modificado] \
      src/aine-binder/binder.cpp \
      --message "Evalúa si el cambio en ATL es aplicable a AINE"

# Generación de stubs de headers
aider src/aine-shim/include/ \
      --message "Genera stubs de headers Linux faltantes según el error de compilación: [error]"
```

---

## Reglas para el agente (incluir en system prompt de Cursor)

```
Eres un asistente experto en sistemas operativos, específicamente en 
porting de código entre Linux y macOS a bajo nivel.

Para este proyecto (AINE):
- El target es SIEMPRE macOS 13+ ARM64 (Apple Silicon)
- Nunca uses APIs Linux directamente — usa las de aine-shim o propón la implementación del shim
- Para IPC, usa Mach messages o sockets Unix, nunca Netlink ni otros mecanismos Linux
- Para threading, usa pthreads con la signatura de macOS (no Linux)
- El build system es CMake + Ninja, no soong/Android.bp
- C++17 para código nuevo, C11 para el shim
- Cuando adaptes código de ATL, identifica explícitamente qué es Linux-específico

Siempre que generes código:
1. Añade un comentario // AINE: si es una adaptación de ATL o Darling
2. Añade un test en tests/ que verifique la funcionalidad
3. Si introduces una nueva dependencia, añádela a scripts/setup-deps.sh
```

---

## Tracking de trabajo con agentes

Mantén un fichero `docs/ai-sessions.md` con entradas como:

```markdown
## 2025-01-15 — Implementación epoll→kqueue

**Agente:** Claude Sonnet via Cursor
**Duración:** ~3h
**Resultado:** aine_epoll_create/ctl/wait implementados y con tests pasando

**Qué funcionó bien:** La traducción EPOLLIN/EPOLLOUT → EVFILT_READ/EVFILT_WRITE
**Qué requirió intervención manual:** El manejo de EPOLLHUP cuando el otro extremo cierra — el comportamiento de kqueue difiere sutilmente
**Commits:** abc1234, def5678
```

Esto ayuda a calibrar qué tareas son buenas candidatas para delegación a agentes en el futuro.
