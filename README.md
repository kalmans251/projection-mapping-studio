# Projection Mapping Studio

Windows용 C++20 / Qt 6 멀티 프로젝터 맵핑 프로토타입.

## v0.2 기능

- Qt 설정 UI, 두 투사 영역의 보정점 드래그 및 JSON 저장
- Windows 확장 디스플레이 목록 탐색 및 **서로 다른 두 출력 장치 선택**
- 선택한 디스플레이에 전체 화면 테스트 격자 출력 (Esc로 닫기)
- Qt Multimedia 단일 비디오 디코더로 MP4 등 영상 불러오기
- 동일한 디코딩 프레임을 2개 출력에 가로 분할하여 표시
- 영상 반복 재생, 재생/일시정지/정지
- GitHub Actions Windows 빌드 및 실행 파일 묶음 아티팩트

**제한:** 초기 기능 검증용 버전이며, 현재 분할 출력은 Qt QPainter 방식입니다. Direct3D 11 GPU 렌더러, 실제 투영 왜곡(Warp) 적용, 밝기 엣지 블렌딩, 하드웨어 출력 동기화는 **아직 미구현**입니다. UI의 보정점과 밝기 값은 실제 영상 출력에 반영되지 않습니다.

## 사용 환경

- Windows 10/11, Visual Studio 2022, CMake 3.24+
- Qt 6.5+ **MSVC 2022 64-bit** 및 **Qt Multimedia** 모듈
- PC에 별도 관리 모니터가 있는 경우 디스플레이 3개 출력 권장
- Windows 설정 → 디스플레이 → 여러 디스플레이를 **확장**으로 설정

## 빌드 (Qt 6.12.0 예시)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/msvc2022_64"
cmake --build build --config Release
& "C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe" --release ".\build\Release\ProjectionMappingStudio.exe"
.\build\Release\ProjectionMappingStudio.exe
```

CMake에서 Multimedia를 찾지 못하면 Qt Maintenance Tool에서 **Qt Multimedia** 모듈을 추가 설치하세요.

## 사용 절차

1. 관리용 모니터에서 프로그램 실행
2. **Projectors** 탭 → Refresh → 출력 디스플레이 2개 선택 (관리용 모니터 제외)
3. **Open test patterns** → 양쪽 화면에 격자 표시 확인
4. **Media** 탭 → 동영상 선택 → Play
5. 영상 종료 시 반복할지 Loop 체크박스로 선택
6. **Close projector windows**로 출력 종료 (또는 투사 화면에서 Esc)

주의: 아직 실제 워핑/엣지 블렌딩이 없어 두 화면의 경계는 보일 수 있습니다. 프로젝터가 겹치는 부분은 밝아질 수 있습니다. 현재 overlap 값은 원본 영상의 분할 영역에만 적용됩니다.
