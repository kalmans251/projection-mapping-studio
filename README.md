# Projection Mapping Studio

Windows C++20 + Qt 6 멀티 프로젝터 맵핑 소프트웨어.

## v0.6 — 실험적 Direct3D 11 출력

기존 Qt CPU 출력 경로는 그대로 유지합니다. 새 GPU 경로는 아직 현장 검증 전입니다.

- Direct3D 11 하드웨어 장치 및 비디오 텍스처 업로드
- 두 프로젝터의 전용 QWindow에 각각 DXGI Swap Chain 생성
- HLSL 픽셀 셰이더를 이용한 4점 역원근 워핑
- 하나의 GPU 비디오 텍스처를 두 출력에서 공유
- 겹침 폭, 밝기, 블렌딩 감마, RGB 감쇠를 GPU 출력에서 처리
- 설정값 변경 시 GPU 화면 재렌더링
- **GPU renderer** 탭에서 GPU 모드 선택. 기본값은 검증된 Qt CPU 렌더러
- GPU 모드 전환 시 기존 출력 창을 닫고 사용자가 다시 열도록 처리

### 주의사항

- GPU 모드는 **실험 기능**입니다. 노트북·GPU·프로젝터 실제 조합에서는 별도 검증이 필요합니다.
- 두 Swap Chain이 동일 디코딩 프레임을 사용하지만 **하드웨어 genlock/frame-lock은 지원하지 않습니다**. 순차 Present로 출력합니다.
- Qt Multimedia 디코딩 결과를 GPU에 업로드하므로 하드웨어 디코딩의 완전한 zero-copy 파이프라인은 아닙니다.
- 현재 디스플레이 변경 시 swap chain 자동 재생성, GPU 장치 손실 복구, 검은색 누설광 교정은 미구현입니다.
- 기존 보정 JSON(v1/v2) 호환성을 유지합니다.

## Windows 빌드

- Visual Studio 2022 MSVC x64, Windows SDK
- Qt 6.12.0 MSVC 2022 64-bit + Qt Multimedia
- CMake 3.24+

```powershell
git pull origin main
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/msvc2022_64"
cmake --build build --config Release
& "C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe" --release ".\build\Release\ProjectionMappingStudio.exe"
.\build\Release\ProjectionMappingStudio.exe
```

## GPU 테스트 절차

1. 기존 CPU/Qt 모드에서 프로젝트 설정과 보정 JSON을 백업.
2. **GPU renderer** → **Use experimental D3D11 renderer** 활성화.
3. 출력 창이 닫히면 **Projectors** 탭에서 두 프로젝터를 선택하고 테스트 격자 열기.
4. 두 화면에 격자가 표시되는지 확인. 워핑 보정점을 움직여 출력이 변형되는지 점검.
5. **Media** 탭에서 MP4 재생 후 연결 영역을 확인.
6. 문제가 있으면 GPU 모드를 해제하고 다시 Projectors에서 출력 창 열기 (Qt 렌더러 복귀).

## 향후 개발

GPU 장치 손실/HDMI 재연결 복구, 프레임 타임스탬프·프레젠트 동기화, 디코딩/렌더링 분리, 장시간 무인 운영 테스트.
