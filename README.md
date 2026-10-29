# Projection Mapping Studio

Windows C++20 / Qt 6 멀티 프로젝터 맵핑 프로그램.

## 현재 버전: v0.5 GPU 기반 구축

기존 v0.4 기능(프로젝터 두 대에 영상 분할 출력, 4점 워핑, 겹침 페이드, RGB/감마, JSON 저장)을 유지합니다.

새로 추가된 Direct3D 11 엔진 기반:
- Microsoft D3D11 하드웨어 장치/컨텍스트 생성
- 사용 GPU 어댑터 이름 및 feature level 진단
- GPU 비디오 텍스처(BGRA) 및 Shader Resource View 생성
- Qt Multimedia가 디코딩한 프레임을 GPU 텍스처에 선택적으로 업로드
- 업로드 프레임 수와 전송량 표시
- **GPU diagnostics** 탭에서 초기화/업로드 실험 수행

**중요:** v0.5에서도 실제 프로젝터 출력은 기존 Qt QPainter 워핑/블렌딩 엔진이 담당합니다. Direct3D 텍스처 업로드는 별도 진단 기능이며, 아직 GPU 셰이더 출력 및 swap chain이 구현된 것은 아닙니다. 업로드를 켜면 CPU→GPU 전송이 추가되므로 성능이 좋아지지 않을 수 있습니다. 기본값은 꺼짐입니다.

## 요구 환경

- Windows 10/11 + MSVC 2022 x64 + Windows SDK
- CMake 3.24+
- Qt 6.5+ Widgets, Multimedia (예: Qt 6.12.0 MSVC 2022 64-bit)

## 빌드

```powershell
git pull origin main
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/msvc2022_64"
cmake --build build --config Release
& "C:\Qt\6.12.0\msvc2022_64\bin\windeployqt.exe" --release ".\build\Release\ProjectionMappingStudio.exe"
.\build\Release\ProjectionMappingStudio.exe
```

## GPU 진단

1. 프로그램 **GPU diagnostics** 탭 열기
2. **Initialize D3D11 hardware GPU** 선택
3. GPU 이름과 feature level 표시 확인
4. (선택) GPU 텍스처 업로드 체크 후 영상 재생, 업로드 프레임 수 증가 확인
5. 기본 프로젝터 출력에는 영향을 주지 않으며, 문제가 있으면 업로드 체크 해제

## 다음 로드맵

- v0.6: GPU swap chain 기반 프로젝터 렌더링, HLSL 워핑·블렌딩
- v0.7: 디코딩·렌더링 스레드 및 프레임 타임라인
- v0.8: 자동 실행, 디스플레이 재연결, 운영 설정
- v0.9: 장시간 안정화·현장 품질 검증

**현재 공개 저장소에는 명시적 소프트웨어 배포 라이선스를 아직 지정하지 않았습니다.**
